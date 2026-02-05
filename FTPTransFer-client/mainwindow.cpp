#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QTimerEvent>
#include <QMessageBox>
#include <QCloseEvent>
#include <QFileDialog>
#include <QStandardPaths>
#include <QFile>


void MainWindow::init_Flist_menu()
{
    QMenu* menu = new QMenu(ui->list_file);

    // 添加菜单项
    auto cdupAction = menu->addAction("返回上一级目录");
    auto refAction = menu->addAction("刷新");
    auto putAction = menu->addAction("上传");
    auto getAction = menu->addAction("下载");
    auto delAction = menu->addAction("删除");

    // 设置上下文菜单策略
    ui->list_file->setContextMenuPolicy(Qt::CustomContextMenu);

    // 连接上下文菜单请求信号
    connect(ui->list_file, &QTableWidget::customContextMenuRequested,[=](const QPoint& pos){
        qDebugTime() << ui->list_file->currentIndex().row();
        menu->exec(ui->list_file->viewport()->mapToGlobal(pos));  // mapToGlobal() 将局部坐标转换为屏幕全局坐标, 这样菜单就可以在正确的位置显示
    });
    connect(cdupAction, &QAction::triggered, this,&MainWindow::cdupAction);
    connect(refAction, &QAction::triggered, this,&MainWindow::refAction);
    connect(putAction, &QAction::triggered, this,&MainWindow::putAction);
    connect(getAction, &QAction::triggered, this,&MainWindow::getAction);
    connect(delAction, &QAction::triggered, this,&MainWindow::delAction);

    // 确保 QTableWidget 可交互
    ui->list_file->setEnabled(true);
    ui->list_file->setSelectionBehavior(QAbstractItemView::SelectRows);

    // 设置表头
    ui->list_file->setColumnCount(7);
    ui->list_file->setHorizontalHeaderLabels({"权限","链接数","属主","属组","大小","日期","文件名"});

    // 获取水平表头
    QHeaderView* header = ui->list_file->horizontalHeader();

    // 设置前6列为根据内容调整，但设置一个最小宽度，避免太窄
    for (int i = 0; i < 6; ++i) {
        header->setSectionResizeMode(i, QHeaderView::ResizeToContents);
        // 可以设置一个最小宽度，避免内容过短时列太窄
        // ui->list_file->setColumnWidth(i, 50); // 如果不想用ResizeToContents，可以设置固定宽度
    }

    // 设置最后一列为拉伸模式，填充剩余空间
    header->setSectionResizeMode(6, QHeaderView::Stretch);

    // 可选：设置列宽的最小值，避免拉伸时过窄
    header->setMinimumSectionSize(80);

    qDebugTime() << "Init_Flist_menu finished";
}





MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , ui_connect_status("  就绪")
    , ui_pwd("  当前路径：/")
    , ui_cur_time(cur_time())
    , ui_ssl_status("  SSL: 关闭")  // 新增
{
    ui->setupUi(this);

    // 设置表格
    init_Flist_menu();

    // 设置状态栏
    ui->statusBar->addWidget(&ui_connect_status);
    ui->statusBar->addWidget(&ui_pwd);
    ui->statusBar->addPermanentWidget(&ui_cur_time);

    // 设置定时器
    timerId = startTimer(1000);

    // 关闭程序
    // 确保菜单栏可见
    this->menuBar()->setVisible(true);
    connect(ui->action_exit, &QAction::triggered, this, [this](){
        qDebug() << "action_exit";
        if(QMessageBox::question(this, this->windowTitle(), "你确定要退出吗？") != QMessageBox::Yes) return;
        exit(0);
    });

    connect(ui->action_connect, &QAction::triggered, this, &MainWindow::onConnect);
    connect(ui->list_file,&QTableWidget::cellDoubleClicked,this,&MainWindow::cellDoubleClicked);

    connect(&ftp, &Ftp::errorOccurred, this, &MainWindow::onFtpError);
    connect(&ftp, &Ftp::sslStatusChanged, this, &MainWindow::onFtpSSLStatusChanged);

    qInfoTime() << "MainWindow::MainWindow() -> finished";

}



QString MainWindow::cur_time()
{
    return QTime::currentTime().toString();
}


void MainWindow::timerEvent(QTimerEvent *e)
{
    if(e->timerId() != timerId){
        qWarningTime() << "timerEvent Error: e->timerId() != timerId";
        return;
    }
    ui_cur_time.setText(cur_time());
}


void MainWindow::closeEvent(QCloseEvent *e)
{
    qDebug() << "closeEvent";
    if(QMessageBox::question(this,this->windowTitle(),"你确定要退出吗？") == QMessageBox::Yes)return;
    e->ignore();
}



MainWindow::~MainWindow()
{
    delete ui;
}


void MainWindow::set_pwd()
{
    QString remote_pwd;
    remote_pwd = ftp.pwd();
    if(remote_pwd.isEmpty()){
        remote_pwd = "获取当前路径失败";
    }
    ui_pwd.setText("当前路径：" + remote_pwd);
}

void MainWindow::clear_ui_list()
{
    // ui->list_file->clearContents();
    ui->list_file->setRowCount(0);
}

void MainWindow::insert_list(const std::vector<FTP_FILE_INFO> &list)
{
    for(auto& info : list)
    {
        insert_row(ui->list_file->rowCount(),info);
    }
}

void MainWindow::insert_row(int row, const FTP_FILE_INFO &info)
{
    ui->list_file->insertRow(row);
    insert_item(row,0,info.access);
    insert_item(row,1,info.link_cnt);
    insert_item(row,2,info.ower);
    insert_item(row,3,info.group);
    insert_item(row,4,info.size);
    insert_item(row,5,info.date);
    insert_item(row,6,info.file_name);
}

void MainWindow::insert_item(int row,int idx,QString item)
{
    ui->list_file->setItem(row,idx,new QTableWidgetItem(item));
    ui->list_file->item(row,idx)->setFlags(Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
}

void MainWindow::onConnect()
{
    FtpConnectDlg dlg(this);
    if(dlg.exec() != QDialog::Accepted) return;

    FTP_DATA data = dlg.ftp_data();
    // 显示连接状态
    ui_connect_status.setText("正在连接 " + data.host + " (SSL:" + (data.useSSL ? "是" : "否") + ")...");

    if(!ftp.login(data)){
        QMessageBox::warning(this, u8"连接失败", u8"连接失败" + ftp.error(), QMessageBox::Ok);
        ui_connect_status.setText("连接失败");
        qWarningTime() << ftp.error();
        return;
    }
    ui_connect_status.setText("连接成功：" + data.host +
                              (data.useSSL ? " (SSL加密)" : " (非加密)"));

    clear_ui_list();         // 清空表格内容
    set_pwd();               // 设置当前路径


    // 尝试获取目录列表，如果失败可能是SSL数据连接问题
    auto list = ftp.dir();
    if(list.empty()) {
        QMessageBox::warning(this, u8"警告",
                             u8"获取目录列表可能失败，SSL数据连接可能有问题",
                             QMessageBox::Ok);
    }

    insert_list(list);  // 插入文件列表内容
}


void MainWindow::cellDoubleClicked(int row, int idx)
{
    // 1. 检查行号有效性
    if(row == -1)return;

    // 2. 获取文件名（第6列）
    QString file_name = ui->list_file->item(row,6)->text();

    // 3. 判断是否是目录并处理文件名
    QString permissions = ui->list_file->item(row, 0)->text();
    bool isDirectory = !permissions.isEmpty() && permissions[0] == 'd';
    if(isDirectory) file_name = file_name.split(" ")[0];
    else return;

    if(file_name == "."){
        return;
    }

    // 4. 尝试切换到该目录
    if(file_name == ".."){
        ftp.cdup();
    }
    else if(!ftp.cd(file_name)){
        QMessageBox::warning(this,this->windowTitle(),
                             QString("%1切换失败，%1可能不是目录").arg(file_name));
        return;
    }

    // 5. 更新当前路径显示
    set_pwd();

    // 6. 获取新目录的文件列表
    auto list = ftp.dir();

    // 7. 清空并刷新UI列表
    clear_ui_list();
    insert_list(list);
}


void MainWindow::cdupAction()
{
    if(ui->list_file->rowCount() == 0){
        return;
    }
    if(!ftp.cdup()){
        QMessageBox::warning(this, u8"提示", u8"返回上一级失败", QMessageBox::Ok);
        return;
    }
    set_pwd();
    clear_ui_list();
    insert_list(ftp.dir());
}

void MainWindow::refAction()
{
    clear_ui_list();
    insert_list(ftp.dir());
}

void MainWindow::putAction()
{
    QFileDialog file(this,"上传文件");
    file.setFileMode(QFileDialog::FileMode::ExistingFile);
    if(file.exec() != QDialog::Accepted)return;
    auto files = file.selectedFiles();
    if(files.size() > 1) {
        QMessageBox::warning(this,"选择文件过多！","最多选择一个文件");
        return;
    }
    qInfoTime() << "MainWindow::putAction() -> 打开的文件：" << files[0];
    if(!ftp.put(files[0])){
        QMessageBox::warning(this,this->windowTitle(),"上传失败\n错误：" + ftp.error());
        return;
    }
    QMessageBox::information(this,this->windowTitle(),"上传成功！");
    clear_ui_list();
    insert_list(ftp.dir());
}

void MainWindow::getAction()
{
    if(ui->list_file->rowCount() == 0){
        return;
    }
    int row = ui->list_file->currentRow();
    if(row == -1){
        QMessageBox::warning(this,this->windowTitle(),"没有选择下载文件");
        return;
    }
    QFileDialog file(this,"下载文件到");
    file.setFileMode(QFileDialog::FileMode::AnyFile);
    QString downloadPath = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    file.setDirectory(downloadPath);
    file.selectFile(ui->list_file->item(row,6)->text());
    qDebugTime() << "MainWindow::getAction() downloadPath: " << downloadPath;
    if(file.exec() != QDialog::Accepted)return;
    auto files = file.selectedFiles();
    if(files.size() > 1) {
        QMessageBox::warning(this,"选择文件过多！","最多选择一个文件");
        return;
    }
    if(QFileInfo(files[0]).isDir()){
        // 目录
        files[0] = files[0] + "/" + ui->list_file->item(row,6)->text();
    }

    qInfoTime() << "MainWindow::getAction() -> 打开的文件：" << files[0];
    if(!ftp.get(files[0], ui->list_file->item(row,6)->text())){
        QMessageBox::warning(this,this->windowTitle(),"下载失败\n错误：" + ftp.error());
        return;
    }
    QMessageBox::information(this,this->windowTitle(),"下载成功！");
}

void MainWindow::delAction()
{
    if(ui->list_file->rowCount() == 0){
        return;
    }
    int row = ui->list_file->currentRow();
    if(row == -1){
        QMessageBox::warning(this,this->windowTitle(),"没有选择删除文件");
        return;
    }
    if(!ftp.del(get_file_name(row)))
    {
        QMessageBox::warning(this,this->windowTitle(),"删除失败\n错误：" + ftp.error());
        return;
    }
    QMessageBox::information(this,this->windowTitle(),"删除成功！");
    clear_ui_list();
    insert_list(ftp.dir());
}

QString MainWindow::get_file_name(int row)
{
    if (row < 0 || row >= ui->list_file->rowCount()) {
        return QString();
    }

    QTableWidgetItem* nameItem = ui->list_file->item(row, 6);
    QTableWidgetItem* permItem = ui->list_file->item(row, 0);

    if (!nameItem || !permItem) {
        return QString();
    }

    QString fileName = nameItem->text();
    QString permissions = permItem->text();

    // 更准确地处理符号链接
    if (!permissions.isEmpty() && permissions[0] == 'l') {
        // 符号链接格式: "linkname -> target"
        if (fileName.contains(" -> ")) {
            fileName = fileName.split(" -> ").first().trimmed();
        }
    }

    return fileName;
}

// 新增槽函数实现
void MainWindow::onFtpError(const QString& error)
{
    QMessageBox::warning(this, "FTP错误", error, QMessageBox::Ok);
}

void MainWindow::onFtpSSLStatusChanged(bool enabled)
{
    ui_ssl_status.setText(enabled ? "  SSL: 已启用" : "  SSL: 未启用");
}
