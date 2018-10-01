#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "defaultBootloaderCode.h"
#include "defaultFlashDriverCode.h"
#include "crc16.h"
#include <QFile>
#include <QFileDialog>
#include <QTextStream>
#include <QDateTime>
#include <QDir>
#include <QLabel>
#include <QProcess>
#include <QMessageBox>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QJsonParseError>
#include <QClipboard>
#include <QDebug>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    //初始化控件
    componentsInitialization();
    //设置布局
    layoutsInitialization();
    //命令行初始化
    commandsInitialization();
}

MainWindow::~MainWindow()
{
    delete ui;
}

//切换功能后，控件间关联关系的处理
void MainWindow::switchFunctionPressed()
{
    switch(m_cmbFunctionSwitch->currentIndex())
    {
    case BOOTLOADER:
        m_cmbFunctionSwitch->setFixedWidth(182);
        m_gbSwitchFunction->setTitle(tr("switch function"));
        m_leRunCommand->setVisible(false);
        m_btnGenerate->setStatusTip(tr("generate firmware with bootloader"));
        m_btnGenerate->setVisible(true);
        m_gbBootloader->setVisible(true);
        m_gbS19Selector->setVisible(true);
        m_gbDiagnosis->setVisible(false);
        ptOutputWnd->setVisible(false);
        break;
    case DIAGNOSIS:
        m_cmbFunctionSwitch->setFixedWidth(182);
        m_gbSwitchFunction->setTitle(tr("switch function"));
        m_leRunCommand->setVisible(false);
        m_btnGenerate->setStatusTip(tr("generate firmware for diagnosis"));
        m_btnGenerate->setVisible(true);
        m_gbBootloader->setVisible(false);
        m_gbS19Selector->setVisible(true);
        m_gbDiagnosis->setVisible(true);
        ptOutputWnd->setVisible(false);
        break;
    case CMD_HANDLER:
        m_cmbFunctionSwitch->setFixedWidth(90);
        m_gbSwitchFunction->setTitle(tr("input command"));
        m_leRunCommand->setVisible(true);
        m_btnGenerate->setVisible(false);
        m_gbBootloader->setVisible(false);
        m_gbS19Selector->setVisible(false);
        m_gbDiagnosis->setVisible(false);
        ptOutputWnd->setVisible(true);
        break;
    default:
        break;
    }
}

void MainWindow::runCmdReturnPressed()
{
    QString inputString = m_leRunCommand->text();
    CmdType findCmd = CMD_MAX;

    foreach(auto pairElem, cmdList)
    {
        foreach(auto strListElem, pairElem.second)
        {
            if(!strListElem.compare(inputString, Qt::CaseInsensitive))
            {
                findCmd = pairElem.first;
                break;
            }
        }

        if(CMD_MAX != findCmd)
            break;
    }

    switch(findCmd)
    {
    case CMD_HELP:
    case CMD_HELP_BOOTLOADER:
    case CMD_HELP_DAIGNOSIS:
        showHelpInfo(findCmd);
        break;
    case CMD_CLEAR_SCREEN:
        ptOutputWnd->clear();
        break;
    case CMD_SAVE_CONFIG_FILE:
    case CMD_LOAD_CONFIG_FILE:
        procConfigFile(findCmd);
        break;
    case CMD_CODE_TO_STRING:
        generateCharArray();
        break;
    case CMD_CRC_CALCULATOR:
        crcTest();
        break;
    case CMD_DIAG_M1A_S021_AUTOFILL:
        m_leDiagnosisS021->setText(DIAG_M1A_S021);
    case CMD_DIAG_M1A_S021:
        ptOutputWnd->clear();
        ptOutputWnd->appendPlainText(DIAG_M1A_S021);
        break;
    case CMD_DIAG_T19_S021_AUTOFILL:
        m_leDiagnosisS021->setText(DIAG_T19_S021);
    case CMD_DIAG_T19_S021:
        ptOutputWnd->clear();
        ptOutputWnd->appendPlainText(DIAG_T19_S021);
        break;
#if WIN32
    case CMD_WINDOWS_CALCULATOR:
    case CMD_WINDOWS_SNIPPINGTOOL:
    case CMD_WINDOWS_PAINT:
    {
        QString windowsCmd = cmdList.at(findCmd).second.first();
        windowsCmd = windowsCmd.right(windowsCmd.size() - 1);
        QProcess::startDetached(windowsCmd);
        break;
    }
#endif
    default:
        ptOutputWnd->clear();
        ptOutputWnd->appendHtml("Unknown command, input <u>:?</u> for help.");
        break;
    }
}

void MainWindow::generateButtonPressed()
{
    switch(m_cmbFunctionSwitch->currentIndex())
    {
    case BOOTLOADER:
        generateFirmwareWithBootloader();
        break;
    case DIAGNOSIS:
        generateFirmwareForDiagnosis();
        break;
    default:
        break;
    }

    qDebug()<<"wd height:"<<this->size().height();
    qDebug()<<"wd width:"<<this->size().width();

    qDebug()<<"m_cmbFunctionSwitch:"<<m_cmbFunctionSwitch->size().width();
}

void MainWindow::useDefaultBootloaderPressed()
{
    if(m_ckbUseDefaultBootloader->isChecked())
    {
        m_btnLoadBootloader->setEnabled(false);
        m_leBootloaderInfo->setText(tr("default boot code is loaded"));
        m_leBootloaderInfo->setEnabled(false);
        m_btnLoadBootloader->setStatusTip(tr("use default boot code now"));
    }
    else
    {
        m_btnLoadBootloader->setEnabled(true);
        m_leBootloaderInfo->setEnabled(true);
        m_leBootloaderInfo->clear();
        m_btnLoadBootloader->setStatusTip(tr("select the bootloader file"));
    }
}

void MainWindow::loadBootloaderPressed()
{
    QString fileName = QFileDialog::getOpenFileName();

    if(!fileName.isEmpty())
        m_leBootloaderInfo->setText(fileName);

    qDebug()<<fileName<<endl;
}

void MainWindow::s021ReturnPressed()
{
    QString originalS021Data = m_leDiagnosisS021->text();

    if(originalS021Data.isEmpty())
    {
        QMessageBox::warning(this, "Warnning", "empty data, couldn't modify version", QMessageBox::Yes);
        return;
    }

    //S021***30302E30312E323040, 至少也要有22个有效字节(实际要更多，这里先这样校验即可)
    if(!originalS021Data.startsWith("S021", Qt::CaseInsensitive) || originalS021Data.size() < DIAG_M1A_S021_MIN_LENGTH)
    {
        QMessageBox::warning(this, "Warnning", "invalid data, couldn't modify version", QMessageBox::Yes);
        return;
    }

    //请求用户输入版本信息数据
    bool isOK;
    QString versionQueryData = QInputDialog::getText(NULL,
                                                     "version data query",
                                                     "Please input new version, format: xx.xx.xx\n",
                                                     QLineEdit::Normal,
                                                     "",
                                                     &isOK);
    //校验输入信息, 严格匹配xx.xx.xx
    QRegExp regExp("^([a-fA-F\\d]{2}\\.){2}[a-fA-F\\d]{2}$");

    if(isOK && regExp.exactMatch(versionQueryData))
    {
        QString newS021String = originalS021Data.left(originalS021Data.size() - 18);
        newS021String += versionQueryData.toLatin1().toHex();
        newS021String += originalS021Data.right(2);

        m_leDiagnosisS021->setText(newS021String.toUpper());
    }
    else
    {
        QMessageBox::warning(this, "Warnning", "invalid version data, version update failed", QMessageBox::Yes);
        return;
    }
}

void MainWindow::selectFilePressed()
{
    //定义文件对话框类
    QFileDialog* fileDialog = new QFileDialog(this);
    //定义文件对话框标题
    fileDialog->setWindowTitle(tr("choose .S19 file"));
    //设置默认文件路径
    fileDialog->setDirectory(".");
    //设置文件过滤器
    fileDialog->setNameFilter(tr("*.S19"));
    //设置可以选择多个文件,默认为只能选择一个文件QFileDialog::ExistingFile
    fileDialog->setFileMode(QFileDialog::ExistingFile);
    //设置视图模式
    fileDialog->setViewMode(QFileDialog::Detail);
    //打印所有选择的文件的路径
    QStringList fileNames;

    if(fileDialog->exec())
    {
        fileNames = fileDialog->selectedFiles();

        fileInfo.clear();
        fileInfo.push_back(fileNames.first());
        fileInfo.push_back(QFileInfo(fileNames.first()).absolutePath());
        fileInfo.push_back(QFileInfo(fileNames.first()).fileName());

        m_leFileInfo->setText(fileNames.first());
    }

    for(auto tmp:fileInfo)
        qDebug()<<tmp<<endl;
}

void MainWindow::generateFirmwareWithBootloader()
{
    if(m_leBootloaderInfo->text().isEmpty())
    {
        QMessageBox::warning(this, "Warnning", "Please select a bootloader file", QMessageBox::Yes);
        return;
    }

    if(m_leFileInfo->text().isEmpty())
    {
        QMessageBox::warning(this, "Warnning", "Please select a .S19 file", QMessageBox::Yes);
        return;
    }

    //获取bootloader文件
    QString bootloaderCodeString;
    if(!m_ckbUseDefaultBootloader->isChecked())
    {
        QFile bootloaderFile(m_leBootloaderInfo->text());
        if(!bootloaderFile.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            QMessageBox::warning(this, "Warnning", "Cannot open " + m_leBootloaderInfo->text(), QMessageBox::Yes);
            return;
        }

        QTextStream bootloaderFileIn(&bootloaderFile);
        QStringList bootloaderCodeStringList;
        while(!bootloaderFileIn.atEnd())
        {
            QString readStr = bootloaderFileIn.readLine();

            if(!readStr.isEmpty())
                bootloaderCodeStringList.push_back(readStr);
        }

        bootloaderFile.close();

        bootloaderCodeString = bootloaderCodeStringList.join('\n');
        bootloaderCodeString += '\n';

        int targetIndex = bootloaderCodeStringList.indexOf(TARGET_STRING_AFTER_GENERATING_BOOTCODE);
        if(-1 == targetIndex)
        {
            QMessageBox::warning(this, "Warnning", "please check the bootloader file, interrupt vector table line doesn't exist", QMessageBox::Yes);
            return;
        }
    }
    else
    {
        bootloaderCodeString = DEFAULT_BOOTLOADER_CODE;
    }

    qDebug()<<bootloaderCodeString<<endl;

    //获取.S19原文件
    QString fileName =fileInfo.at(ABSOLUTE_FILE_PATH);
    QFile s19File(fileName);
    if(!s19File.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QMessageBox::warning(this, "Warnning", "Cannot open " + fileName, QMessageBox::Yes);
        return;
    }
    QTextStream s19FileIn(&s19File);
    QStringList originalS19FileStringList;
    while(!s19FileIn.atEnd())
    {
        QString readStr = s19FileIn.readLine();

        if(!readStr.isEmpty())
            originalS19FileStringList.push_back(readStr);
    }
    s19File.close();

    for(auto& elem:originalS19FileStringList)
        elem += '\n';

    //将bootloader合成进原始S19文件
    int targetIndex = originalS19FileStringList.indexOf(TARGET_STRING_AFTER_GENERATING_BOOTCODE);
    if(-1 != targetIndex)
    {
        QMessageBox::warning(this, "Warnning", "please check the .S19 file, maybe bootloader is already exist", QMessageBox::Yes);
        return;
    }

    int replaceIndex = originalS19FileStringList.indexOf(REPLACE_STRING);
    if(-1 == replaceIndex)
    {
        QMessageBox::warning(this, "Warnning", "please check the .S19 file, interrupt vector table line doesn't exist", QMessageBox::Yes);
        return;
    }
    originalS19FileStringList.replace(replaceIndex, bootloaderCodeString);

    //存储生成的含bootloader的文件
    QString tmpFileName = fileInfo.at(FILE_NAME);
    QString timeInfo = QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss");
    tmpFileName = tmpFileName.left(tmpFileName.size() - 4);
    tmpFileName += "_withBootloader_" + timeInfo + ".S19";

    qDebug()<<tmpFileName<<endl;

    QString folderName = "/generatedFirmwaresWithBootloader/";
    QString dirPath = fileInfo.at(ABSOLUTE_PATH) + folderName;
    QDir dir(dirPath);
    if(!dir.exists())
        dir.mkdir(dirPath);

    qDebug()<<dirPath<<endl;

    QString newFilePathName = dirPath + tmpFileName;
    QFile newFile(newFilePathName);
    if(!newFile.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QMessageBox::warning(this, "Warnning", "Cannot open " + newFilePathName, QMessageBox::Yes);
        return;
    }

    QTextStream out(&newFile);
    for(auto elem:originalS19FileStringList)
    {
        if(elem != "\n")
            out << elem;
    }
    newFile.close();

    qDebug()<<newFilePathName<<endl;

    //WINDOWS环境下，选中该文件
#ifdef WIN32
    QProcess process;
    QString openFileName = newFilePathName;

    openFileName.replace("/", "\\");    //***这句windows下必要***
    process.startDetached("explorer /select," + openFileName);
#endif
}

//生成诊断仪用flash driver
void MainWindow::generateFlashDriverForDiagnosis(QString dir_path)
{
    QString filePathName = dir_path + "CherryT19SeriesFlashDriver.S19";

    QDir dir(dir_path);
    if(!dir.exists())
        dir.mkdir(dir_path);

    //删除临时文件
    QFile tmpFile(filePathName);
    if (tmpFile.exists())
    {
        tmpFile.remove();
    }

    QFile newFile(filePathName);
    if(!newFile.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QMessageBox::warning(this, "Warnning", "Cannot open " + filePathName, QMessageBox::Yes);
        return;
    }

    QTextStream out(&newFile);
    out << DEFAULT_FLASHDRIVER_CODE;

    newFile.close();
}

//生成诊断仪用固件
void MainWindow::generateFirmwareForDiagnosis()
{
    if(m_leFileInfo->text().isEmpty())
    {
        QMessageBox::warning(this, "Warnning", "Please select a .S19 file", QMessageBox::Yes);
        return;
    }

    if(m_leDiagnosisS021->text().isEmpty())
    {
        QMessageBox::warning(this, "Warnning", "Please input S021 data", QMessageBox::Yes);
        return;
    }
    else
    {
        if(!m_leDiagnosisS021->text().startsWith("S021", Qt::CaseInsensitive) || m_leDiagnosisS021->text().size() < DIAG_M1A_S021_MIN_LENGTH)
        {
            QMessageBox::warning(this, "Warnning", "Please check S021 data", QMessageBox::Yes);
            return;
        }
    }

    //获取.S19原文件
    QString fileName =fileInfo.at(ABSOLUTE_FILE_PATH);
    QFile s19File(fileName);
    if(!s19File.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QMessageBox::warning(this, "Warnning", "Cannot open " + fileName, QMessageBox::Yes);
        return;
    }
    QTextStream s19FileIn(&s19File);
    QStringList originalS19FileStringList;
    while(!s19FileIn.atEnd())
    {
        QString readStr = s19FileIn.readLine();

        if(!readStr.isEmpty())
            originalS19FileStringList.push_back(readStr);
    }
    s19File.close();

    for(auto& elem:originalS19FileStringList)
        elem += '\n';

    //校验原文件是否正确
    int bootValidateIndex = originalS19FileStringList.indexOf(TARGET_STRING_AFTER_GENERATING_BOOTCODE);
    if(-1 != bootValidateIndex)
    {
        QMessageBox::warning(this, "Warnning", "please check the .S19 file, bootloader code exists", QMessageBox::Yes);
        return;
    }

    if(originalS19FileStringList.first().startsWith("S021", Qt::CaseInsensitive))
    {
        QMessageBox::warning(this, "Warnning", "please check the .S19 file, S021 code is already exist in first line", QMessageBox::Yes);
        return;
    }

    if(originalS19FileStringList.at(originalS19FileStringList.size() - 1).startsWith("S20CFE", Qt::CaseInsensitive))
    {
        QMessageBox::warning(this, "Warnning", "please check the .S19 file, CRC code is already exist in first line", QMessageBox::Yes);
        return;
    }

    int vectorValidateIndex = originalS19FileStringList.indexOf(REPLACE_STRING);
    if(-1 == vectorValidateIndex)
    {
        QMessageBox::warning(this, "Warnning", "please check the .S19 file, interrupt vector table S10B line doesn't exist", QMessageBox::Yes);
        return;
    }

    //替换S0行，删除S10B行
    originalS19FileStringList.replace(0, m_leDiagnosisS021->text() + '\n');
    originalS19FileStringList.removeOne(REPLACE_STRING);

    for(auto elem: originalS19FileStringList)
        qDebug() << elem << endl;

    //.S19文件中S2按Fx升序排序
    if(!sortS19Code(originalS19FileStringList))
    {
        return;
    }

    //生成临时文件
    QString tmpFileName = fileInfo.at(FILE_NAME);
    tmpFileName = tmpFileName.left(tmpFileName.size() - 4);
    tmpFileName += "_diagnosis(tmp).S19";

    QString folderName = "/generatedFirmwaresForDiagnosis/";
    QString dirPath = fileInfo.at(ABSOLUTE_PATH) + folderName;
    QDir dir(dirPath);
    if(!dir.exists())
        dir.mkdir(dirPath);

    QString tmpFilePathName = dirPath + tmpFileName;
    QFile newFile(tmpFilePathName);
    if(!newFile.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QMessageBox::warning(this, "Warnning", "Cannot open " + tmpFilePathName, QMessageBox::Yes);
        return;
    }

    QTextStream out(&newFile);
    for(auto elem:originalS19FileStringList)
    {
        if(elem != "\n")
            out << elem;
    }
    newFile.close();

    //将临时文件的路径复制到系统剪贴板
    QClipboard *clipBoard = QApplication::clipboard();
    clipBoard->setText(dirPath);

    //清空上次输入的S20CFE数据
    m_leDiagnosisS20C->clear();

    //请求用户输入S20CFE数据
    bool isOK;
    QString s20cQuery = QInputDialog::getText(NULL, "CRC data query",
                                               "Please input CRC result code starts with S20CFE, using file shown as below, " \
                                               "file directory path has been copied to system clipboard:\n" + tmpFilePathName,
                                               QLineEdit::Normal,
                                               "",
                                               &isOK);

    if(isOK && s20cQuery.startsWith("S20CFE", Qt::CaseInsensitive))
    {
        s20cQuery = s20cQuery.toUpper();
        m_leDiagnosisS20C->setText(s20cQuery);
        originalS19FileStringList.insert(originalS19FileStringList.size() - 1, s20cQuery + '\n');

        //删除临时文件
        QFile tmpFile(tmpFilePathName);
        if (tmpFile.exists())
        {
            tmpFile.remove();
        }

        //生成固件并在文件夹中定位此文件
        QString newFilePathName = tmpFilePathName.left(tmpFilePathName.size() - 9);
        QString timeInfo = QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss");
        newFilePathName += "_" + timeInfo + ".S19";

        QFile newFile(newFilePathName);
        if(!newFile.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            QMessageBox::warning(this, "Warnning", "Cannot open " + newFilePathName, QMessageBox::Yes);
            return;
        }

        QTextStream out(&newFile);
        for(auto elem:originalS19FileStringList)
        {
            if(elem != "\n")
                out << elem;
        }
        newFile.close();

        //同时生成一个flash driver
        generateFlashDriverForDiagnosis(dirPath);
#ifdef WIN32
        QProcess process;
        QString openNewFileName = newFilePathName;

        openNewFileName.replace("/", "\\");    //***这句windows下必要***
        process.startDetached("explorer /select," + openNewFileName);
#endif
    }
    else
    {
        QMessageBox::warning(this, "Warnning", "please input correct CRC data", QMessageBox::Yes);

        //删除临时文件
        QFile tmpFile(tmpFilePathName);
        if (tmpFile.exists())
        {
            tmpFile.remove();
        }

        return;
    }
}

bool MainWindow::sortS19Code(QStringList &originalStringList)
{
    QStringList stringListS0AndS9;
    QStringList stringListS1;
    QStringList stringListS2[16];

    //S0、S9行
    stringListS0AndS9.push_back(originalStringList.first().toUpper());
    stringListS0AndS9.push_back(originalStringList.last());

    for(QString elem: originalStringList)
    {
        //S1代码段
        if(elem.startsWith("S1", Qt::CaseInsensitive))
        {
            stringListS1.push_back(elem);
        }

        if(elem.startsWith("S2", Qt::CaseInsensitive))
        {
            //S2代码段，开头为：S2**Fx, x在0-F之间
            int index = hexCharToHex(elem.at(5).toLatin1());

            if(index >= 0 && index < 16)
            {
                stringListS2[index].push_back(elem);
            }
            else
            {
                QMessageBox::warning(this, "Warnning", "check:" + elem, QMessageBox::Yes);
                return false;
            }

        }
    }

    //重组originalStringList,按S0-S1-S2-S9的顺序
    originalStringList.clear();
    originalStringList.push_front(stringListS0AndS9.first());
    originalStringList += stringListS1;
    for(int cnt = 0; cnt < 16; ++cnt)
    {
        if(!stringListS2[cnt].isEmpty())
            originalStringList += stringListS2[cnt];
    }
    originalStringList.push_back(stringListS0AndS9.last());

    return true;
}

int MainWindow::hexCharToHex(char src)
{
    int ret = -1;
    src = toupper(src);

    if(src >= '0' && src <= '9')
    {
        ret = src - '0';
    }
    else if(src >= 'A' && src <= 'F')
    {
        ret = src - 'A' + 10;
    }

    return ret;
}

//计算CRC，参考《ECU bootloader and programming implementation specification》
unsigned int MainWindow::calcCRC(unsigned int size, QString fileData)
{
    unsigned int crc = 0xffff; /* initial value */
    unsigned char tmp = 0;
    unsigned int i = 0;

    QByteArray charArray = fileData.toLatin1();

    for(i = 0; i < size; i++)
    {
        if('\n' != charArray.at(i))
        {
            tmp=(crc>>8)^charArray.at(i);
            crc=(crc<<8)^crcLookupTable[tmp];
        }
    }

    return crc;
}

void MainWindow::crcTest()
{
    QString filePathName = QFileDialog::getOpenFileName();
    QString filePath = QFileInfo(filePathName).absolutePath();

    qDebug()<<filePathName<<endl<<filePath<<endl;

    if(filePathName.isEmpty())
    {
        QMessageBox::warning(this, "Warnning", "generate failed, please select a file", QMessageBox::Yes);
        return;
    }

    QFile targetFile(filePathName);
    if(!targetFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QMessageBox::warning(this, "Warnning", "Cannot open " + filePathName, QMessageBox::Yes);
        return;
    }

    QTextStream targetFileIn(&targetFile);
    QStringList targetCodeStringList;
    while(!targetFileIn.atEnd())
    {
        QString readStr = targetFileIn.readLine();

        if(!readStr.isEmpty())
            targetCodeStringList.push_back(readStr);
    }

    targetFile.close();

    QString file = targetCodeStringList.join('\n');

    unsigned int crc = calcCRC(file.size(), file);

    ptOutputWnd->clear();
    ptOutputWnd->appendPlainText(file);

    QString outStr;
    outStr += "\n\n######################\n";
    outStr += "filesize: " + QString::number(file.size()) + "\n";
    outStr += "crc: " + QString::number(crc) + "\n";
    outStr += "crc high: " + QString::number((crc>>8)&0xff) + "\n";
    outStr += "crc low: " + QString::number(crc&0xff) + "\n";
    outStr += "######################";
    ptOutputWnd->appendPlainText(outStr);

    qDebug() << crc << "----" << ((crc>>8)&0xff) <<"----"<< (crc&0xff) <<endl;
}

void MainWindow::showHelpInfo(CmdType cmd)
{
    QStringList hlpInfo;

    if(CMD_HELP == cmd)
    {
        hlpInfo.push_back(tr("0.Welcome to use and spread this open source serial port tool."));
        hlpInfo.push_back(tr("1.This tool is developed under Qt creator using QT5 in C++, thanks for QT's easy-use."));
        hlpInfo.push_back(tr("2.Input <u>:show extra</u> or <u>:se</u> for extra features, and <u>:hide extra</u> or <u>:he</u> to hide them."));
        hlpInfo.push_back(tr("3.Input <u>:save config file</u> or <u>:scf</u> to save config file, and <u>:load config file</u> or <u>:lcf</u> to load config file, "
                             "take care that the tool will not save or load config file automatically."));
        hlpInfo.push_back(tr("4.Input <u>:reset</u> or <u>:rst</u> to reset the tool, and <u>:clear input</u> or <u>:ci</u> to clear extra input area."));
        hlpInfo.push_back(tr("5.Any good idea to improve this tool, click contact author."));
    }
    else if(CMD_HELP_BOOTLOADER == cmd)
    {
        hlpInfo.push_back(tr("《BootLoader合成工具使用方法》"));
        hlpInfo.push_back(tr("0 将<u>switch function</u>切换至<u>add bootloader to firmware</u>."));
        hlpInfo.push_back(tr("1 加载bootloader代码段的两种方式."));
        hlpInfo.push_back(tr("1.1 方式一：加载默认bootloader代码段，只需勾选<u>default</u>， 程序默认设置其为勾选状态."));
        hlpInfo.push_back(tr("1.2 方式二：加载其它bootloader代码段，去勾选<u>default</u>，点击<u>load bootloader</u>按钮选择其它bootloader文件."));
        hlpInfo.push_back(tr("2 点击<u>load file</u>按钮选择.S19原app文件."));
        hlpInfo.push_back(tr("3 点击<u>generate</u>按钮生成含bootloader的新app文件,并自动打开该文件所在的目录且选中该文件."));
    }
    else if(CMD_HELP_DAIGNOSIS == cmd)
    {
        hlpInfo.push_back(tr("《诊断仪app生成工具使用方法》"));
        hlpInfo.push_back(tr("0 将<u>switch function</u>切换至<u>gen firmware for diagnosis</u>."));
        hlpInfo.push_back(tr("1 点击<u>load file</u>按钮选择.S19原app文件."));
        hlpInfo.push_back(tr("2 输入S021数据，该数据最终将位于app的第一行."));
        hlpInfo.push_back(tr("2.1 在命令行可查询程序预置的S021数据，请在命令行输入<u>:?</u>获取相关命令信息."));
        hlpInfo.push_back(tr("2.2 正确输入S021数据后，将光标置于S021数据所在的输入框后，点击回车键可以修改版本号，版本号格式需严格匹配<u>xx.xx.xx</u>,x为0-9或a-f,字母不区分大小写，最终按大写字母写入."));
        hlpInfo.push_back(tr("3 点击<u>generate</u>按钮生成用于计算CRC的临时文件，弹出请求S20C数据的对话框，并将临时文件的路径存入系统剪贴板."));
        hlpInfo.push_back(tr("4 用第三方软件对临时文件计算CRC并生成S20C数据，该数据最终将位于app的倒数第二行，将S20C结果回填至对话框."));
        hlpInfo.push_back(tr("5 点击对话框<u>OK</u>按钮，生成诊断仪app文件,并自动打开该文件所在的目录且选中该文件."));
    }

    ptOutputWnd->clear();
    foreach(auto elem, hlpInfo)
    {
        ptOutputWnd->appendHtml(elem);
    }

    //光标移动到最顶部，否则将显示文件末尾
    QTextCursor cursor = ptOutputWnd->textCursor();
    cursor.movePosition(QTextCursor::Start);
    ptOutputWnd->setTextCursor(cursor);
}

void MainWindow::procConfigFile(CmdType cmd)
{
    if(CMD_SAVE_CONFIG_FILE == cmd)
    {
        QString fileName = QDir::currentPath() + '/' + CONFIG_FILE_NAME;
        QFile file(fileName);
        if(!file.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            QMessageBox::warning(this, "Warnning", "Cannot open " + fileName, QMessageBox::Yes);
            return;
        }

        QJsonObject jsonSettings;
        jsonSettings.insert("ComName", "json test");

        QJsonObject jsonEdit;
        jsonEdit.insert("Showhex", true);

        QJsonArray jsonExtraHexBox;
        for(auto i = 0; i < 10; ++i)
        {
            jsonExtraHexBox.append(i);
        }

        QJsonObject jsonConfig;
        jsonConfig.insert("Settings", jsonSettings);
        jsonConfig.insert("Edit", jsonEdit);
        jsonConfig.insert("ExtraHexBox", jsonExtraHexBox);

        QJsonDocument document;
        document.setObject(jsonConfig);
        QByteArray byte_array = document.toJson(QJsonDocument::Indented);
        QString jsonEncodedString(byte_array);

        QTextStream out(&file);
        out << jsonEncodedString;
        file.close();
    }
    else if(CMD_LOAD_CONFIG_FILE == cmd)
    {
        QString fileName = QDir::currentPath() + '/' + CONFIG_FILE_NAME;
        QFile file(fileName);
        if(!file.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            QMessageBox::warning(this, "Warnning", "Cannot open " + fileName, QMessageBox::Yes);
            return;
        }

        QTextStream in(&file);
        QString jsonEncodedString = in.readAll();
        file.close();

        QJsonParseError jsonError;
        QJsonDocument parseDoucment = QJsonDocument::fromJson(jsonEncodedString.toLocal8Bit(), &jsonError);
        if(QJsonParseError::NoError == jsonError.error)
        {
            if(parseDoucment.isObject())
            {
                QJsonObject docObj = parseDoucment.object();
                if(docObj.contains("Settings"))
                {
                    QJsonValue value = docObj.take("Settings");
                    if(value.isObject())
                    {
                        QJsonObject settingsObj = value.toObject();

                        if(settingsObj.contains("ComName"))
                        {
                            QJsonValue value = settingsObj.take("ComName");
                            if(value.isString())
                            {
                                ptOutputWnd->appendPlainText(value.toString() + '\n');
                            }
                        }
                    }
                }

                if(docObj.contains("Edit"))
                {
                    QJsonValue value = docObj.take("Edit");
                    if(value.isObject())
                    {
                        QJsonObject editObj = value.toObject();

                        if(editObj.contains("Showhex"))
                        {
                            QJsonValue value = editObj.take("Showhex");
                            if(value.isBool())
                            {
                                ptOutputWnd->appendPlainText(QString::number(value.toBool()) + '\n');
                            }
                        }
                    }
                }

                if(docObj.contains("ExtraHexBox"))
                {
                    QJsonValue value = docObj.take("ExtraHexBox");
                    if(value.isArray())
                    {
                        QJsonArray exHexBoxArray = value.toArray();
                        for(auto i = 0; i < 10; ++i)
                        {
                            ptOutputWnd->appendPlainText(QString::number(exHexBoxArray.at(i).toInt()) + ' ');
                        }
                    }
                }
            }
        }
    }
}

//将boot code生成为字符串常量，当boot code更新时，调用此函数将其转换为数组
void MainWindow::generateCharArray()
{
    QString filePathName = QFileDialog::getOpenFileName();
    QString filePath = QFileInfo(filePathName).absolutePath();

    qDebug()<<filePathName<<endl<<filePath<<endl;

    if(filePathName.isEmpty())
    {
        QMessageBox::warning(this, "Warnning", "generate failed, please select a file", QMessageBox::Yes);
        return;
    }

    QFile targetFile(filePathName);
    if(!targetFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QMessageBox::warning(this, "Warnning", "Cannot open " + filePathName, QMessageBox::Yes);
        return;
    }

    QTextStream targetFileIn(&targetFile);
    QStringList targetCodeStringList;
    while(!targetFileIn.atEnd())
    {
        QString readStr = targetFileIn.readLine();

        if(!readStr.isEmpty())
            targetCodeStringList.push_back(readStr);
    }

    targetFile.close();
    for(auto& elem:targetCodeStringList)
    {
        elem.push_front('\"');
        elem.push_back('\\');
        elem.push_back('n');
        elem.push_back('\"');
        elem.push_back('\n');
    }
    if(!targetCodeStringList.isEmpty())
        targetCodeStringList.last().insert(targetCodeStringList.last().size() - 1, ';');

    targetCodeStringList.push_front("const char* DEFAULT_BOOTLOADER_CODE =\n");
    targetCodeStringList.push_front("#define __DEFAULT_BOOTLOADER_CODE_H__\n\n");
    targetCodeStringList.push_front("#ifndef __DEFAULT_BOOTLOADER_CODE_H__\n");
    targetCodeStringList.push_back("\n#endif\n");

    for(auto& elem:targetCodeStringList)
        qDebug()<<elem<<endl;

    QString tmpFileName = "defaultBootloaderCode_";
    QString timeInfo = QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss");
    tmpFileName += timeInfo + ".h";

    QString newFilePathName = filePath + '/' + tmpFileName;
    QFile newFile(newFilePathName);
    if(!newFile.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QMessageBox::warning(this, "Warnning", "Cannot open " + newFilePathName, QMessageBox::Yes);
        return;
    }

    QTextStream out(&newFile);
    for(auto elem:targetCodeStringList)
    {
        if(elem != "\n")
            out << elem;
    }

    newFile.close();

    qDebug()<<newFilePathName<<endl;

    //WINDOWS环境下，选中该文件
#ifdef WIN32
    QProcess process;
    QString openFileName = newFilePathName;

    openFileName.replace("/", "\\");    //***这句windows下必要***
    process.startDetached("explorer /select," + openFileName);
#endif
}

void MainWindow::componentsInitialization(void)
{
    setWindowTitle(tr("CherryT19SeriesFirmwareGenerator"));

    //窗体名称及状态栏设置
    auto labelAuthorInfo = new QLabel;
    labelAuthorInfo->setStatusTip(tr("click to view source code on github"));
    labelAuthorInfo->setOpenExternalLinks(true);
    labelAuthorInfo->setText(QString::fromLocal8Bit("<style> a {text-decoration: none} </style> <a href = https://www.github.com/bingshuizhilian/QTPROJECTS> contact author </a>"));
    labelAuthorInfo->show();
    ui->statusBar->addPermanentWidget(labelAuthorInfo);

    //选择.S19文件按钮
    m_btnChooseFile = new QPushButton(tr("load file"));
    connect(m_btnChooseFile, &m_btnChooseFile->clicked, this, &selectFilePressed);
    m_btnChooseFile->setStatusTip(tr("select the original .S19 file"));
    //显示.S19文件名
    m_leFileInfo = new QLineEdit;
    m_leFileInfo->setReadOnly(true);

    //选择bootloader文件按钮
    m_btnLoadBootloader = new QPushButton(tr("load bootloader"));
    connect(m_btnLoadBootloader, &m_btnLoadBootloader->clicked, this, &loadBootloaderPressed);
    //显示bootloader文件名，或提示正在使用默认boot code
    m_leBootloaderInfo = new QLineEdit;
    m_leBootloaderInfo->setReadOnly(true);

    //是否使用默认boot code选项
    m_ckbUseDefaultBootloader = new QCheckBox(tr("default"));
    connect(m_ckbUseDefaultBootloader, &m_ckbUseDefaultBootloader->stateChanged, this, &useDefaultBootloaderPressed);
    m_ckbUseDefaultBootloader->setStatusTip(tr("use default boot code when checked"));
    m_ckbUseDefaultBootloader->setChecked(true);

    //生成按钮
    m_btnGenerate = new QPushButton(tr("generate"));
    connect(m_btnGenerate, &m_btnGenerate->clicked, this, &generateButtonPressed);

    //合成诊断仪firmware时需要的额外信息
    m_leDiagnosisS021 = new QLineEdit;
    connect(m_leDiagnosisS021, &m_leDiagnosisS021->returnPressed, this, &s021ReturnPressed);
    m_leDiagnosisS021->setStatusTip("press enter to modify version");
    m_leDiagnosisS20C = new QLineEdit;
    m_leDiagnosisS20C->setReadOnly(true);
    m_leDiagnosisS20C->setStatusTip("obtain from 3rd party software");

    //命令行输入输出窗口
    m_leRunCommand = new QLineEdit;
    connect(m_leRunCommand, &m_leRunCommand->returnPressed, this, &runCmdReturnPressed);
    m_leRunCommand->setStatusTip("press enter to run command");
    ptOutputWnd = new QPlainTextEdit;
    ptOutputWnd->setReadOnly(true);
    ptOutputWnd->setStatusTip(tr("execute result echo window"));

    //布局控件
    m_gbBootloader = new QGroupBox;
    m_gbBootloader->setTitle(tr("bootloader settings"));
    m_gbS19Selector = new QGroupBox;
    m_gbS19Selector->setTitle(tr("select .S19 file"));
    m_gbDiagnosis = new QGroupBox;
    m_gbDiagnosis->setTitle(tr("diagnosis data settings"));
    m_gbSwitchFunction = new QGroupBox;

    //功能选择下拉框
    m_cmbFunctionSwitch = new QComboBox;
    connect(m_cmbFunctionSwitch, static_cast<void (QComboBox::*)(const QString&)>(&QComboBox::currentTextChanged), this, &switchFunctionPressed);
    m_cmbFunctionSwitch->setInsertPolicy(QComboBox::NoInsert);
    m_cmbFunctionSwitch->setStatusTip("select a function");
    m_cmbFunctionSwitch->addItem(FUNCTION_STRING_LIST.at(BOOTLOADER), FUNCTION_STRING_LIST.at(BOOTLOADER));
    m_cmbFunctionSwitch->addItem(FUNCTION_STRING_LIST.at(DIAGNOSIS), FUNCTION_STRING_LIST.at(DIAGNOSIS));
    m_cmbFunctionSwitch->addItem(FUNCTION_STRING_LIST.at(CMD_HANDLER), FUNCTION_STRING_LIST.at(CMD_HANDLER));
}

void MainWindow::layoutsInitialization()
{
    //bootloader控件区
    auto layoutBootloader = new QGridLayout;
    auto labelLeBootloader = new QLabel(tr("bootloader"));
    layoutBootloader->addWidget(labelLeBootloader, 0, 0);
    layoutBootloader->addWidget(m_leBootloaderInfo, 0, 1);
    layoutBootloader->addWidget(m_ckbUseDefaultBootloader, 1, 0);
    layoutBootloader->addWidget(m_btnLoadBootloader, 1, 1);
    m_gbBootloader->setLayout(layoutBootloader);

    //select file控件区
    auto layoutS19Selector = new QGridLayout;
    layoutS19Selector->addWidget(m_btnChooseFile, 0, 0);
    layoutS19Selector->addWidget(m_leFileInfo, 0, 1);
    m_gbS19Selector->setLayout(layoutS19Selector);

    //diagnosis控件区
    auto layoutDiagnosis = new QGridLayout;
    auto labelLeS021 = new QLabel(tr("S021:"));
    labelLeS021->setStatusTip("the first line of the firmware");
    auto labelLeS20C = new QLabel(tr("S20C:"));
    labelLeS20C->setStatusTip("the second last line of the firmware");
    layoutDiagnosis->addWidget(labelLeS021, 0, 0);
    layoutDiagnosis->addWidget(m_leDiagnosisS021, 0, 1);
    layoutDiagnosis->addWidget(labelLeS20C, 1, 0);
    layoutDiagnosis->addWidget(m_leDiagnosisS20C, 1, 1);
    m_gbDiagnosis->setLayout(layoutDiagnosis);

    //功能选择控件区
    auto swFuncLayout = new QHBoxLayout;
    swFuncLayout->addWidget(m_cmbFunctionSwitch);
    swFuncLayout->addWidget(m_leRunCommand);
    m_gbSwitchFunction->setLayout(swFuncLayout);

    //底部控件区
    auto bottomLayout = new QHBoxLayout;
    bottomLayout->addWidget(m_gbSwitchFunction);
    bottomLayout->addWidget(m_btnGenerate);

    //globle layout
    m_layoutGlobal = new QVBoxLayout;
    m_layoutGlobal->addWidget(ptOutputWnd);
    m_layoutGlobal->addWidget(m_gbBootloader);
    m_layoutGlobal->addWidget(m_gbS19Selector);
    m_layoutGlobal->addWidget(m_gbDiagnosis);
    m_layoutGlobal->addLayout(bottomLayout);

    //设置窗口和其它组件大小
    m_btnGenerate->setFixedHeight(50);
    //m_leRunCommand->setFixedWidth(200);
    this->setFixedSize(WINDOW_WIDTH, WINDOW_HEIGHT);
    ui->centralWidget->setLayout(m_layoutGlobal);
}

void MainWindow::commandsInitialization()
{
    cmdList.push_back({ CMD_HELP, {":help", ":hlp", ":?"} });
    cmdList.push_back({ CMD_HELP_BOOTLOADER, {":bootloader?", ":b?"} });
    cmdList.push_back({ CMD_HELP_DAIGNOSIS, {":diagnosis?", ":d?"} });
    cmdList.push_back({ CMD_CLEAR_SCREEN, {":clear screen", ":cs"} });
    cmdList.push_back({ CMD_SAVE_CONFIG_FILE, {":save config file", ":scf"} });
    cmdList.push_back({ CMD_LOAD_CONFIG_FILE, {":load config file", ":lcf"} });
    cmdList.push_back({ CMD_CODE_TO_STRING, {":convert code to string", ":c2s"} });
    cmdList.push_back({ CMD_CRC_CALCULATOR, {":crc calc", ":crc"} });
    cmdList.push_back({ CMD_DIAG_M1A_S021, {":m1a s021", ":ms0"} });
    cmdList.push_back({ CMD_DIAG_M1A_S021_AUTOFILL, {":m1a s021 fill", ":ms0f"} });
    cmdList.push_back({ CMD_DIAG_T19_S021, {":t19 s021", ":ts0"} });
    cmdList.push_back({ CMD_DIAG_T19_S021_AUTOFILL, {":t19 s021 fill", ":ts0f"} });
#if WIN32
    //此处要把windows能识别的命令放在stringlist的首位
    cmdList.push_back({ CMD_WINDOWS_CALCULATOR, {":calc", ":calculator"} });
    cmdList.push_back({ CMD_WINDOWS_SNIPPINGTOOL, {":snippingtool", ":snip"} });
    cmdList.push_back({ CMD_WINDOWS_PAINT, {":mspaint", ":mspt"} });
#endif
}
