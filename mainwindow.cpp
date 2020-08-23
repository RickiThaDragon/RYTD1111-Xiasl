#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QPainter>
#include <QTextBlock>

//线程中使用
bool loading = true;
bool thread_end = true; //线程是否结束
QTreeWidget *treeWidgetBack;
QsciScintilla *textEditBack;


thread_one::thread_one(QObject *parent) : QThread(parent)
{
}

CodeEditor::CodeEditor(QWidget *parent) : QPlainTextEdit(parent)
{
    lineNumberArea = new LineNumberArea(this);

    connect(this, &CodeEditor::blockCountChanged, this, &CodeEditor::updateLineNumberAreaWidth);
    connect(this, &CodeEditor::updateRequest, this, &CodeEditor::updateLineNumberArea);
    connect(this, &CodeEditor::cursorPositionChanged, this, &CodeEditor::highlightCurrentLine);

    updateLineNumberAreaWidth(0);
    highlightCurrentLine();
}


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    ver = "QtiASL V1.0.3    ";
    setWindowTitle(ver);

    //线程
    mythread = new thread_one();
    connect(mythread,&thread_one::over,this,&MainWindow::dealover);//线程结束

    //主菜单
    connect(ui->actionOpen, &QAction::triggered, this, &MainWindow::btnOpen_clicked);
    connect(ui->actionSave, &QAction::triggered, this, &MainWindow::btnSave_clicked);
    connect(ui->actionSaveAs, &QAction::triggered, this, &MainWindow::btnSaveAs_clicked);
    connect(ui->actionAbout, &QAction::triggered, this, &MainWindow::about);

    connect(ui->actionGenerate, &QAction::triggered, this, &MainWindow::btnGenerate_clicked);
    connect(ui->actionCompiling, &QAction::triggered, this, &MainWindow::btnCompile_clicked);

    //textEdit = new CodeEditor(this);
    textEdit = new QsciScintilla(this);
    textEditBack = new QsciScintilla();//用于后台处理数据

    //connect(textEdit, SIGNAL(textChanged()), this, SLOT(documentWasModified()));
    connect(textEdit, &QsciScintilla::cursorPositionChanged, this, &MainWindow::textEdit_cursorPositionChanged);
    connect(textEdit, &QsciScintilla::textChanged, this, &MainWindow::textEdit_textChanged);
    connect(textEdit, &QsciScintilla::linesChanged, this, &MainWindow::textEdit_linesChanged);


#ifdef Q_OS_WIN32
// win
    //QFont font("SauceCodePro Nerd Font", 9, QFont::Normal);
    font.setFamily("SauceCodePro Nerd Font");
    font.setPointSize(9);

#endif

#ifdef Q_OS_LINUX
// linux
    font.setFamily("SauceCodePro Nerd Font");
    font.setPointSize(13);
#endif

#ifdef Q_OS_MAC
// mac
    font.setFamily("SauceCodePro Nerd Font");
    font.setPointSize(13);

    ui->actionGenerate->setEnabled(false);

#endif

   init_edit();

   int w = screen()->size().width();
   treeWidgetBack = new QTreeWidget();//后台处理数据
   init_treeWidget(treeWidgetBack, w);
   init_treeWidget(ui->treeWidget, w);

    //textEdit->setLineWrapMode(textEdit->NoWrap);
    ui->editShowMsg->setLineWrapMode(ui->editShowMsg->NoWrap);
    ui->editShowMsg->setReadOnly(true);
    ui->editShowMsg->setMaximumHeight(200);
    ui->editShowMsg->setMinimumHeight(100);


    splitterMain = new QSplitter(Qt::Horizontal,this);
    splitterMain->addWidget(ui->treeWidget);
    QSplitter *splitterRight = new QSplitter(Qt::Vertical,splitterMain);
    splitterRight->setOpaqueResize(true);
    splitterRight->addWidget(textEdit);
    splitterRight->addWidget(ui->editShowMsg);
    ui->gridLayout->addWidget(splitterMain);

    //MyHighLighter *highlighter;
    //highlighter = new MyHighLighter();
    //highlighter->setDocument(textEdit->document());

    //联动判断
    timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(timer_linkage()));



}

MainWindow::~MainWindow()
{
    delete ui;

    mythread->quit();
    mythread->wait();


}
void MainWindow::about()
{
    QFileInfo appInfo(qApp->applicationFilePath());
    QString last = tr("最后的编译时间(Last modified): ") + appInfo.lastModified().toString("yyyy-MM-dd hh:mm:ss");

    QMessageBox::about(this , tr("About") ,
                       QString::fromLocal8Bit("<a style='color: blue;' href = https://github.com/ic005k/QtiASL>QtiASL Editor</a>") + last);
}
void MainWindow::loadFile(const QString &fileName)
{
   loading = true;

    QFile file(fileName);
    if (!file.open(QFile::ReadOnly | QFile::Text)) {
        QMessageBox::warning(this, tr("Application"),
                             tr("Cannot read file %1:\n%2.")
                             .arg(QDir::toNativeSeparators(fileName), file.errorString()));
        return;
    }

    QTextStream in(&file);
#ifndef QT_NO_CURSOR
    QGuiApplication::setOverrideCursor(Qt::WaitCursor);
#endif
    //textEdit->setPlainText(in.readAll());

    textEdit->setText(QString::fromUtf8(file.readAll()));

#ifndef QT_NO_CURSOR
    QGuiApplication::restoreOverrideCursor();
#endif

    setCurrentFile(fileName);
    statusBar()->showMessage(tr("File loaded"), 2000);

    ui->editShowMsg->clear();

    ui->treeWidget->clear();
    ui->treeWidget->repaint();
    on_btnRefreshTree_clicked();
    ui->treeWidget->repaint();

    //刷新树表,装入文件不采用线程，采用阻塞方式，以让文件正常读入
    //textEditBack->clear();
    //textEditBack->setText(textEdit->text());
    //refreshTree(treeWidgetBack);
    //dealover();


}

void MainWindow::setCurrentFile(const QString &fileName)
{
    curFile = fileName;
    //textEdit->document()->setModified(false);
    textEdit->setModified(false);
    setWindowModified(false);

    QString shownName = curFile;
    if (curFile.isEmpty())
        shownName = "untitled.txt";
    setWindowFilePath(shownName);

    setWindowTitle(ver + shownName);
}


void MainWindow::btnOpen_clicked()
{
    if (maybeSave())
    {
        QString fileName = QFileDialog::getOpenFileName(this,"DSDT文件","","DSDT文件(*.aml *.dsl *.dat);;所有文件(*.*)");
        if (!fileName.isEmpty())
        {
            QFileInfo fInfo(fileName);
            if(fInfo.suffix() == "aml" || fInfo.suffix() == "dat")
            {
                QFileInfo appInfo(qApp->applicationDirPath());
                #ifdef Q_OS_WIN32
                // win
                    QProcess::execute(appInfo.filePath() + "/iasl.exe" , QStringList() << "-d" << fileName);
                #endif

                #ifdef Q_OS_LINUX
                // linux
                    QProcess::execute(appInfo.filePath() + "/iasl" , QStringList() << "-d" << fileName);

                #endif

                #ifdef Q_OS_MAC
                // mac
                    QProcess::execute(appInfo.filePath() + "/iasl" , QStringList() << "-d" << fileName);
                #endif


                fileName = fInfo.path() + "/" + fInfo.baseName() + ".dsl";

            }

            loadFile(fileName);




        }


    }


}

bool MainWindow::maybeSave()
{
    if (!textEdit->isModified())
        return true;
    const QMessageBox::StandardButton ret
        = QMessageBox::warning(this, tr("Application"),
                               tr("The document has been modified.\n"
                                  "Do you want to save your changes?"),
                               QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    switch (ret) {
    case QMessageBox::Save:
        return btnSave_clicked();
    case QMessageBox::Cancel:
        return false;
    default:
        break;
    }
    return true;
}

bool MainWindow::btnSave_clicked()
{
    if (curFile.isEmpty()) {
        return btnSaveAs_clicked();
    } else {
        return saveFile(curFile);
    }
}

bool MainWindow::btnSaveAs_clicked()
{
    QFileDialog dialog;
    QString fn = dialog.getSaveFileName(this,"DSDT","","DSDT(*.dsl);;All(*.*)");
    if(fn.isEmpty())
        return false;

    return  saveFile(fn);

}

bool MainWindow::saveFile(const QString &fileName)
{
    QString errorMessage;

    QGuiApplication::setOverrideCursor(Qt::WaitCursor);
    QSaveFile file(fileName);
    if (file.open(QFile::WriteOnly | QFile::Text)) {

        //textEdit->SendScintilla(QsciScintilla::SCI_SETSAVEPOINT);

        QTextStream out(&file);
        out << textEdit->text();
        if (!file.commit()) {
            errorMessage = tr("Cannot write file %1:\n%2.")
                           .arg(QDir::toNativeSeparators(fileName), file.errorString());
        }
    } else {
        errorMessage = tr("Cannot open file %1 for writing:\n%2.")
                       .arg(QDir::toNativeSeparators(fileName), file.errorString());
    }
    QGuiApplication::restoreOverrideCursor();

    if (!errorMessage.isEmpty()) {
        QMessageBox::warning(this, tr("Application"), errorMessage);
        return false;
    }

    setCurrentFile(fileName);
    statusBar()->showMessage(tr("File saved"), 2000);

    //刷新成员树
    on_btnRefreshTree_clicked();

    return true;
}

void MainWindow::btnGenerate_clicked()
{
    QFileInfo appInfo(qApp->applicationDirPath());
    qDebug() << appInfo.filePath();
    //QString mydir = appInfo.filePath() + "/mydsdt";
    //QDir dir;
    //if(dir.mkpath(mydir))
    //{

    //}

    QProcess dump;
    QProcess iasl;


#ifdef Q_OS_WIN32
// win
    dump.execute(appInfo.filePath() + "/acpidump.exe" , QStringList() << "-b");//阻塞
    iasl.execute(appInfo.filePath() + "/iasl.exe" , QStringList() << "-d" << "dsdt.dat");
#endif

#ifdef Q_OS_LINUX
// linux
    dump.execute(appInfo.filePath() + "/acpidump" , QStringList() << "-b");
    iasl.execute(appInfo.filePath() + "/iasl" , QStringList() << "-d" << "dsdt.dat");

#endif

#ifdef Q_OS_MAC
// mac
    dump.execute(appInfo.filePath() + "/acpidump" , QStringList() << "-b");
    iasl.execute(appInfo.filePath() + "/iasl" , QStringList() << "-d" << "dsdt.dat");

#endif



    //qDebug() << iasl.waitForStarted();

    loadFile(appInfo.filePath() + "/dsdt.dsl");


}

void MainWindow::btnCompile_clicked()
{
    QFileInfo cf_info(curFile);
    if(cf_info.suffix().toLower() != "dsl")
        return;

    QFileInfo appInfo(qApp->applicationDirPath());
    co = new QProcess;

    if(!curFile.isEmpty())
        btnSave_clicked();

#ifdef Q_OS_WIN32
// win
   co->start(appInfo.filePath() + "/iasl.exe" , QStringList() << "-f" << curFile);
#endif

#ifdef Q_OS_LINUX
// linux
   co->start(appInfo.filePath() + "/iasl" , QStringList() << "-f" << curFile);

#endif

#ifdef Q_OS_MAC
// mac
    co->start(appInfo.filePath() + "/iasl" , QStringList() << "-f" << curFile);

#endif

    connect(co , SIGNAL(finished(int)) , this , SLOT(readResult(int)));
    //connect(co , SIGNAL(readyReadStandardOutput()) , this , SLOT(readResult(int)));

    //QByteArray res = co.readAllStandardOutput(); //获取标准输出
    //qDebug() << "Out" << QString::fromLocal8Bit(res);

}

void MainWindow::setMark()
{
    //回到第一行
    QTextBlock block = ui->editShowMsg->document()->findBlockByNumber(0);
    ui->editShowMsg->setTextCursor(QTextCursor(block));

    //将"error"高亮
    QString search_text = "Error";
        if (search_text.trimmed().isEmpty())
        {
            QMessageBox::information(this, tr("Empty search field"), tr("The search field is empty."));
        }
        else
        {
            QTextDocument *document = ui->editShowMsg->document();
            bool found = false;
            QTextCursor highlight_cursor(document);
            QTextCursor cursor(document);

            //开始
            cursor.beginEditBlock();
            QTextCharFormat color_format(highlight_cursor.charFormat());
            color_format.setForeground(Qt::red);
            color_format.setBackground(Qt::yellow);


            while (!highlight_cursor.isNull() && !highlight_cursor.atEnd())
            {
                //查找指定的文本，匹配整个单词
                highlight_cursor = document->find(search_text, highlight_cursor, QTextDocument::FindCaseSensitively);
                if (!highlight_cursor.isNull())
                {
                    if(!found)
                        found = true;


                    highlight_cursor.mergeCharFormat(color_format);


                }
            }


           cursor.endEditBlock();

        }
}

void MainWindow::readResult(int exitCode)
{

    QTextCodec* gbkCodec = QTextCodec::codecForName("UTF-8");
    ui->editShowMsg->clear();
    QString result = gbkCodec->toUnicode(co->readAll());
    ui->editShowMsg->append(result);
    ui->editShowMsg->append(co->readAllStandardError());

    //回到第一行
    QTextBlock block = ui->editShowMsg->document()->findBlockByNumber(0);
    ui->editShowMsg->setTextCursor(QTextCursor(block));

    //清除所有标记
    textEdit->SendScintilla(QsciScintilla::SCI_MARKERDELETEALL);

    if(exitCode == 0)
    {

        ui->btnPreviousError->setEnabled(false);
        ui->btnNextError->setEnabled(false);

        QMessageBox::information(this , "编译(Compiling)" , "编译成功(Compiled successfully)");

    }
    else
    {
        ui->btnPreviousError->setEnabled(true);
        ui->btnNextError->setEnabled(true);

        on_btnNextError_clicked();
    }

    //setMark();


}

void MainWindow::textEdit_cursorPositionChanged()
{
    int ColNum , RowNum;
    textEdit->getCursorPosition(&RowNum , &ColNum);

    ui->statusbar->showMessage(QString::number(RowNum + 1) + "    " + QString::number(ColNum));

    //联动treeWidget
    if(ui->treeWidget->isVisible())
        mem_linkage(ui->treeWidget);
    else
        mem_linkage(treeWidgetBack);


}

void MainWindow::timer_linkage()
{
    if(!loading)
    {
         on_btnRefreshTree_clicked();
         timer->stop();

    }

}

void MainWindow::mem_linkage(QTreeWidget * tw)
{

    int ColNum , RowNum;
    textEdit->getCursorPosition(&RowNum , &ColNum);

    if(!loading && tw->topLevelItemCount() > 0)
    {



        int cu; //当前位置
        int cu1;//下一个位置

        for(int i = 0; i < tw->topLevelItemCount(); i++)
        {
            cu = tw->topLevelItem(i)->text(1).toInt();

            if(i + 1 == tw->topLevelItemCount())
            {
                cu1 = cu;
            }
            else
            {
                cu1 = tw->topLevelItem(i + 1)->text(1).toInt();
            }


            if(RowNum == cu)
            {

                tw->setCurrentItem(tw->topLevelItem(i));

                break;

            }
            else
            {
                if(RowNum > cu && RowNum < cu1)
                {
                    tw->setCurrentItem(tw->topLevelItem(i));
                    break;
                }

            }

         }

        //最后一个,单独处理
        int t = tw->topLevelItemCount();
        cu = tw->topLevelItem(t - 1)->text(1).toUInt();
        if(RowNum > cu)
        {

           tw->setCurrentItem(tw->topLevelItem(t - 1));

        }


    }

}


int CodeEditor::lineNumberAreaWidth()
{
    int digits = 1;
    int max = qMax(1, blockCount());
    while (max >= 10) {
        max /= 10;
        ++digits;
    }

    int space = 3 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;

    return space;
}

void CodeEditor::updateLineNumberAreaWidth(int /* newBlockCount */)
{
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

void CodeEditor::updateLineNumberArea(const QRect &rect, int dy)
{
    if (dy)
        lineNumberArea->scroll(0, dy);
    else
        lineNumberArea->update(0, rect.y(), lineNumberArea->width(), rect.height());

    if (rect.contains(viewport()->rect()))
        updateLineNumberAreaWidth(0);
}

void CodeEditor::resizeEvent(QResizeEvent *e)
{
    QPlainTextEdit::resizeEvent(e);

    QRect cr = contentsRect();
    lineNumberArea->setGeometry(QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
}


void CodeEditor::lineNumberAreaPaintEvent(QPaintEvent *event)
{
    QPainter painter(lineNumberArea);
    painter.fillRect(event->rect(), Qt::lightGray);

//![extraAreaPaintEvent_0]

//![extraAreaPaintEvent_1]
    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + qRound(blockBoundingRect(block).height());
//![extraAreaPaintEvent_1]

//![extraAreaPaintEvent_2]
    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            QString number = QString::number(blockNumber + 1);
            painter.setPen(Qt::black);
            painter.drawText(0, top, lineNumberArea->width(), fontMetrics().height(),
                             Qt::AlignRight, number);
        }

        block = block.next();
        top = bottom;
        bottom = top + qRound(blockBoundingRect(block).height());
        ++blockNumber;
    }
}

void CodeEditor::highlightCurrentLine()
{
    QList<QTextEdit::ExtraSelection> extraSelections;

    if (!isReadOnly()) {
        QTextEdit::ExtraSelection selection;

        QColor lineColor = QColor(Qt::yellow).lighter(160);

        selection.format.setBackground(lineColor);
        selection.format.setProperty(QTextFormat::FullWidthSelection, true);
        selection.cursor = textCursor();
        selection.cursor.clearSelection();
        extraSelections.append(selection);
    }

    setExtraSelections(extraSelections);


}


void MainWindow::on_btnReplace_clicked()
{
    //textEdit->textCursor().insertText(ui->editReplace->text().trimmed());
    textEdit->replace(ui->editReplace->text());

}

void MainWindow::on_editShowMsg_textChanged()
{

}

void MainWindow::on_btnFindNext_clicked()
{

    //查找,第四个bool为是否循环查找
    textEdit->findFirst(ui->editFind->text().trimmed() , true , false , false , false);


}

void MainWindow::on_btnFindPrevious_clicked()
{

    //查找
    //textEdit->find(ui->editFind->text().trimmed() , QTextDocument::FindBackward);

    if(textEdit->findFirst(ui->editFind->text().trimmed() , true , false , false , false, false))
    //if(textEdit->findFirstInSelection(ui->editFind->text(), true, false, false, false))
        textEdit->findNext();

}

void MainWindow::on_treeWidget_itemClicked(QTreeWidgetItem *item, int column)
{


    if(column == 0)
    {
        int lines = item->text(1).toInt();
        textEdit->setCursorPosition(lines , 0);
        textEdit->setFocus();

        //QTreeWidgetItem *item = ui->treeWidget->currentItem();
        //item->setBackgroundColor(0, QColor(Qt::blue).lighter(150));

    }



}

void MainWindow::treeWidgetBack_itemClicked(QTreeWidgetItem *item, int column)
{


    if(column == 0)
    {
        int lines = item->text(1).toInt();
        textEdit->setCursorPosition(lines , 0);
        textEdit->setFocus();

        //QTreeWidgetItem *item = ui->treeWidget->currentItem();
        //item->setBackgroundColor(0, QColor(Qt::blue).lighter(150));

    }



}

void MainWindow::on_btnRefreshTree_clicked()
{

    if(!thread_end)
        return;

    //将textEdit的内容读到后台
    textEditBack->clear();
    textEditBack->setText(textEdit->text());

    //splitterMain->replaceWidget(0, ui->treeWidget);

    mythread->start();

}

void getMembers(QString str_member, QsciScintilla *textEdit, QTreeWidget *treeWidget)
{

    QString str;
    int RowNum, ColNum;
    int count;  //总行数
    QTreeWidgetItem *twItem0;

    count = textEdit->lines();

    //回到第一行
    textEdit->setCursorPosition(0, 0);

    for(int j = 0; j < count; j++)
    {
        //正则、区分大小写、匹配整个单词、循环搜索
        if(textEdit->findFirst(str_member , true , true , true , false))
        {

           textEdit->getCursorPosition(&RowNum, &ColNum);

           str = textEdit->text(RowNum);

           //qDebug() << "str" << str;

           QString space = findKey(str, str_member.mid(0, 1), 4);

           QString sub = str.trimmed();
           //qDebug() << "sub" << sub;

           bool zs = false;//当前行是否存在注释
           for(int k = ColNum; k > -1; k--)
           {
               if(str.mid(k - 2 , 2) == "//" || str.mid(k - 2 , 2) == "/*")
               {
                   zs = true;
                   //qDebug() << str.mid(k - 2 , 2);
                   break;
               }
           }

           QString str_end;
           if(sub.mid(0 , str_member.count()) == str_member && !zs)
           {

               for(int i = 0; i < sub.count(); i++)
               {
                   if(sub.mid(i , 1) == ")")
                   {
                       str_end = sub.mid(0 , i + 1);

                       twItem0 = new QTreeWidgetItem(QStringList() << space + str_end << QString("%1").arg(RowNum, 7, 10, QChar('0')));//QString::number(RowNum));
                       QFont font;
                       QColor color;
                       if(str_member == "Scope")
                       {

                           font.setBold(true);
                           twItem0->setFont(0 , font);
                       }
                       if(str_member == "Method")
                       {

                           //font.setItalic(true);
                           //twItem0->setFont(0 , font);

                           color = QColor(34, 139, 34);
                           twItem0->setForeground(0, color);

                       }
                       if(str_member == "Name")
                       {

                           font.setItalic(true);
                           twItem0->setFont(0 , font);

                           //color = QColor(34, 139, 34);
                           //twItem0->setForeground(0, color);

                       }
                       if(str_member == "Device")
                       {

                           //color = QColor(Qt::yellow).lighter(150);
                           color = QColor(160, 32, 240 );
                           twItem0->setForeground(0, color);

                       }

                       treeWidget->addTopLevelItem(twItem0);

                       //qDebug() <<"strend" << str_end;

                       break;

                   }
               }


           }
         }
        else
            break;
    //qDebug() << "Hi" << j;
    }

}

void MainWindow::on_editShowMsg_cursorPositionChanged()
{
    QList<QTextEdit::ExtraSelection> extraSelection;
    QTextEdit::ExtraSelection selection;
    QColor lineColor = QColor(255,255,0, 50);
    //QColor lineColor = QColor(Qt::red).lighter(150);
    selection.format.setBackground(lineColor);
    selection.format.setProperty(QTextFormat::FullWidthSelection,true);
    selection.cursor = ui->editShowMsg->textCursor();
    //selection.cursor.clearSelection();
    extraSelection.append(selection);
    ui->editShowMsg->setExtraSelections(extraSelection);
}

void MainWindow::on_btnNextError_clicked()
{

    const QTextCursor cursor = ui->editShowMsg->textCursor();
    int RowNum = cursor.blockNumber();

    QTextBlock block = ui->editShowMsg->document()->findBlockByNumber(RowNum);
    ui->editShowMsg->setTextCursor(QTextCursor(block));


    for(int i = RowNum + 1; i < ui->editShowMsg->document()->lineCount(); i++)
    {
        QTextBlock block = ui->editShowMsg->document()->findBlockByNumber(i);
        ui->editShowMsg->setTextCursor(QTextCursor(block));


        QString str = ui->editShowMsg->document()->findBlockByLineNumber(i).text();
        QString sub = str.trimmed();


        if(sub.mid(0 , 5) == "Error")
        {
            QTextBlock block = ui->editShowMsg->document()->findBlockByNumber(i);
            ui->editShowMsg->setTextCursor(QTextCursor(block));

            QList<QTextEdit::ExtraSelection> extraSelection;
            QTextEdit::ExtraSelection selection;
            //QColor lineColor = QColor(Qt::gray).lighter(150);
            QColor lineColor = QColor(Qt::red);
            selection.format.setForeground(Qt::white);
            selection.format.setBackground(lineColor);
            selection.format.setProperty(QTextFormat::FullWidthSelection,true);
            selection.cursor = ui->editShowMsg->textCursor();
            selection.cursor.clearSelection();
            extraSelection.append(selection);
            ui->editShowMsg->setExtraSelections(extraSelection);

            //定位到错误行
            getErrorLine(i);

            break;

        }



        if(i == ui->editShowMsg->document()->lineCount() - 1)
            on_btnPreviousError_clicked();
    }

}

void MainWindow::on_btnPreviousError_clicked()
{

    const QTextCursor cursor = ui->editShowMsg->textCursor();
    int RowNum = cursor.blockNumber();

    QTextBlock block = ui->editShowMsg->document()->findBlockByNumber(RowNum);
    ui->editShowMsg->setTextCursor(QTextCursor(block));


    for(int i = RowNum - 1; i > -1; i--)
    {
        QTextBlock block = ui->editShowMsg->document()->findBlockByNumber(i);
        ui->editShowMsg->setTextCursor(QTextCursor(block));


        QString str = ui->editShowMsg->document()->findBlockByLineNumber(i).text();
        QString sub = str.trimmed();

        if(sub.mid(0 , 5) == "Error")
        {
            QTextBlock block = ui->editShowMsg->document()->findBlockByNumber(i);
            ui->editShowMsg->setTextCursor(QTextCursor(block));

            QList<QTextEdit::ExtraSelection> extraSelection;
            QTextEdit::ExtraSelection selection;
            //QColor lineColor = QColor(Qt::gray).lighter(150);
            QColor lineColor = QColor(Qt::red);
            selection.format.setForeground(Qt::white);
            selection.format.setBackground(lineColor);
            selection.format.setProperty(QTextFormat::FullWidthSelection,true);
            selection.cursor = ui->editShowMsg->textCursor();
            selection.cursor.clearSelection();
            extraSelection.append(selection);
            ui->editShowMsg->setExtraSelections(extraSelection);

            //定位到错误行
            getErrorLine(i);

            break;

        }

        if(i == 0)
            on_btnNextError_clicked();
    }

}

void MainWindow::getErrorLine(int i)
{
    //定位到错误行
    QString str1 = ui->editShowMsg->document()->findBlockByLineNumber(i - 1).text().trimmed();
    QString str2 , str3;
    if(str1 != "")
    {
        for(int j = 3; j < str1.count(); j++)
        {

            if(str1.mid(j , 1) == ":")
            {
                str2 = str1.mid(0 , j);

                break;
            }
        }

        for(int k = str2.count(); k > 0; k--)
        {
            if(str2.mid(k - 1 , 1) == " ")
            {
                str3 = str2.mid(k , str2.count() - k);

                //定位到错误行
                textEdit->setCursorPosition(str3.toInt() - 1 , 0);
                int linenr = str3.toInt();
                //SCI_MARKERGET 参数用来设置标记，默认为圆形标记
                textEdit->SendScintilla(QsciScintilla::SCI_MARKERGET, linenr - 1);
                //SCI_MARKERSETFORE，SCI_MARKERSETBACK设置标记前景和背景标记
                textEdit->SendScintilla(QsciScintilla::SCI_MARKERSETFORE, 0, QColor(Qt::red));
                textEdit->SendScintilla(QsciScintilla::SCI_MARKERSETBACK, 0, QColor(Qt::red));
                textEdit->SendScintilla(QsciScintilla::SCI_MARKERADD, linenr - 1);
                //下划线
                //textEdit->SendScintilla(QsciScintilla::SCI_STYLESETUNDERLINE,linenr,true);
                //textEdit->SendScintilla(QsciScintilla::SCI_MARKERDEFINE,0,QsciScintilla::SC_MARK_UNDERLINE);

                textEdit->setFocus();

                break;
            }
        }
    }
}

void MainWindow::on_editShowMsg_selectionChanged()
{
    QString row = ui->editShowMsg->textCursor().selectedText();
    int row_num = row.toUInt();
    if(row_num > 0)
    {
        //QTextBlock block = textEdit->document()->findBlockByNumber(row_num - 1);

        //textEdit->setTextCursor(QTextCursor(block));

        textEdit->setCursorPosition(row_num - 1 , 0);

        textEdit->setFocus();


    }


}

void MainWindow::textEdit_textChanged()
{
    if(!loading)
    {
        //on_btnRefreshTree_clicked();
    }
}

void MainWindow::on_editFind_returnPressed()
{
    on_btnFindNext_clicked();
}

const char * QscilexerCppAttach::keywords(int set) const
{
    if(set == 1 || set == 3)
        return QsciLexerCPP::keywords(set);
    if(set == 2)
        return
        //下面是自定义的想要有特殊颜色的关键字
        "External Scope Device Method Name If While Break Return ElseIf Switch Case Else Default Field OperationRegion Package DefinitionBlock Offset CreateDWordField CreateBitField CreateWordField CreateQWordField ";


    return 0;
}

QString findKey(QString str, QString str_sub, int f_null)
{
    int total , tab_count;
    QString strs , space;
    tab_count = 0;
    for(int i = 0; i < str.count(); i++)
    {
        if(str.mid(i, 1) == str_sub)
        {
            strs = str.mid(0, i);


            for(int j = 0; j < strs.count(); j++)
            {
                if(strs.mid(j, 1) == "\t")
                {
                    tab_count = tab_count + 1;

                }
                //qDebug() <<"\t个数：" << strs.mid(j, 1) << tab_count;
            }

            int str_space =  strs.count() - tab_count;
            total = str_space + tab_count * 4 - f_null;

            for(int k = 0; k < total; k++)
                space = space + " ";


            break;

        }
    }

    return space;
}

void MainWindow::textEdit_linesChanged()
{

    //qDebug() << "行号已更改  " + QTime::currentTime().toString();
    timer->start(2000);

}

void thread_one::run()
{

    //在线程中刷新成员列表
    //QMetaObject::invokeMethod(this->parent(),"refreshTree");

    qDebug() << "线程开始 " + QTime::currentTime().toString();
    thread_end = false;

    refreshTree(treeWidgetBack);

    //sleep(10);



    emit over();  //发送完成信号
}

void MainWindow::dealover()//线程结束信号
{


    //splitterMain->replaceWidget(0, treeWidgetBack);

    update_ui_tw();

    thread_end = true;
    mythread->quit();  //test
    mythread->wait();



    qDebug() << "线程结束";

}

void MainWindow::update_ui_tw()
{
    ui->treeWidget->clear();

    ui->treeWidget->update();
    //ui->treeWidget->repaint();


    for(int i = 0; i < treeWidgetBack->topLevelItemCount(); ++i)
    {

        QTreeWidgetItem *item  = treeWidgetBack->topLevelItem(i);

        QString str_member = item->text(0).trimmed().mid(0, 4);
        QTreeWidgetItem *twItem0 = new QTreeWidgetItem(QStringList() << item->text(0) << item->text(1));//QString::number(RowNum));
        QFont font;
        QColor color;
        if(str_member == "Scop" && ui->chkScope->isChecked())
        {

            font.setBold(true);
            twItem0->setFont(0 , font);

            ui->treeWidget->addTopLevelItem(twItem0);
        }
        if(str_member == "Meth" && ui->chkMethod->isChecked())
        {

            //font.setItalic(true);
            //twItem0->setFont(0 , font);

            color = QColor(34, 139, 34);
            twItem0->setForeground(0, color);

            ui->treeWidget->addTopLevelItem(twItem0);

        }
        if(str_member == "Devi" && ui->chkDevice->isChecked())
        {

            //color = QColor(Qt::yellow).lighter(150);
            color = QColor(160, 32, 240 );
            twItem0->setForeground(0, color);

            ui->treeWidget->addTopLevelItem(twItem0);

        }

        if(str_member == "Name" && ui->chkName->isChecked())
        {


            color = QColor(30, 144, 255);
            twItem0->setForeground(0, color);

            ui->treeWidget->addTopLevelItem(twItem0);

        }



        //qDebug() << i << item->text(0) << item->text(1);


    }


    //ui->treeWidget->repaint();
    ui->treeWidget->update();

    textEdit_cursorPositionChanged();
}

void refreshTree(QTreeWidget *treeWidgetBack)
{
    loading = true;

    treeWidgetBack->clear();

#ifdef Q_OS_WIN32
// win

#endif

#ifdef Q_OS_LINUX
// linux

#endif

#ifdef Q_OS_MAC
// mac
   treeWidgetBack->repaint();//mac下重要


#endif



    //枚举"Scope"
    getMembers("Scope", textEditBack, treeWidgetBack);

    //枚举"Device"
    getMembers("Device", textEditBack, treeWidgetBack);

    //枚举"Method"
    getMembers("Method", textEditBack, treeWidgetBack);

    //枚举"Name"
    getMembers("Name", textEditBack, treeWidgetBack);

    //ui->treeWidget->header()->sortIndicatorOrder();
    treeWidgetBack->sortItems(1 , Qt::AscendingOrder);//排序


#ifdef Q_OS_WIN32
// win

#endif

#ifdef Q_OS_LINUX
// linux

#endif

#ifdef Q_OS_MAC
// mac
   treeWidgetBack->repaint();//mac下重要


#endif


    loading = false;

    qDebug() << "刷新成员列表已完成 " + QTime::currentTime().toString();



}

void MainWindow::on_MainWindow_destroyed()
{


}

void MainWindow::init_edit()
{
    //设置字体
    textEdit->setFont(font);
    textEdit->setMarginsFont(font);
    //QFontMetrics fontmetrics = QFontMetrics(font);

     //设置语言解析器
     //QsciLexerBatch *textLexer = new QsciLexerBatch(this);
     //textEdit->setLexer(textLexer);

     QscilexerCppAttach *textLexer1 = new QscilexerCppAttach;
     //获取背景色
     QPalette pal = this->palette();
     QBrush brush = pal.window();
     int red = brush.color().red();
     if(red < 55) //暗模式，mac下为50
     {
         textLexer1->setColor(QColor(255,255,255, 255), -1);
         textLexer1->setColor(QColor(Qt::green), QsciLexerCPP::CommentLine);//"//"注释颜色
         textLexer1->setColor(QColor(Qt::yellow), QsciLexerCPP::Number);
         textLexer1->setColor(QColor(Qt::green), QsciLexerCPP::Comment);
         //textLexer1->setColor(QColor(Qt::red), QsciLexerCPP::Operator);
         //textLexer1->setColor(QColor(Qt::red), QsciLexerCPP::EscapeSequence);

     }
     textLexer1->setColor(QColor(160,32,250, 255), QsciLexerCPP::KeywordSet2);
     textLexer1->setFont(font);
     textEdit->setLexer(textLexer1);
     //textLexer1->setPaper(QColor(255,255,255, 255));//文本区域背景色

     //设置行号栏宽度、颜色
     textEdit->setMarginWidth(0, 70);
     textEdit->setMarginLineNumbers(0, true);
     if(red < 55) //暗模式，mac下为50
     {
         textEdit->setMarginsBackgroundColor(QColor(50, 50 ,50));
         textEdit->setMarginsForegroundColor(Qt::white);
     }

     //设置编码为UTF-8
     textEdit->SendScintilla(QsciScintilla::SCI_SETCODEPAGE,QsciScintilla::SC_CP_UTF8);

     //匹配大小括弧
     textEdit->setBraceMatching(QsciScintilla::SloppyBraceMatch);
     //textEdit->setBraceMatching(QsciScintilla::StrictBraceMatch);


     textEdit->setTabWidth(4);

     //设置括号等自动补全
     textEdit->setAutoIndent(true);
     textEdit->setTabIndents(true);//True如果行前空格数少于tabWidth，补齐空格数,False如果在文字前tab同true，如果在行首tab，则直接增加tabwidth个空格

     //代码提示
     QsciAPIs *apis = new QsciAPIs(textLexer1);
     apis->add(QString("Device"));
     apis->add(QString("Scope"));
     apis->add(QString("Method"));
     apis->add(QString("Return"));
     apis->add(QString("CreateWordField"));
     apis->add(QString("If"));
     apis->add(QString("Else"));
     apis->add(QString("Switch"));
     apis->add(QString("Case"));
     apis->add(QString("Zero"));
     apis->add(QString("Name"));
     apis->add(QString("Field"));
     apis->add(QString("Offset"));
     apis->add(QString("Buffer"));
     apis->add(QString("DefinitionBlock"));
     apis->add(QString("External"));
     apis->add(QString("Arg"));
     apis->add(QString("Alias"));
     apis->add(QString("Package"));
     apis->add(QString("Serialized"));
     apis->add(QString("NotSerialized"));
     apis->add(QString("ToInteger"));
     apis->add(QString("ToUUID"));
     apis->add(QString("Local"));
     apis->prepare();

     //设置自动补全
     textEdit->setCaretLineVisible(true);
     //Acs[None|All|Document|APIs]
     //禁用自动补全提示功能、所有可用的资源、当前文档中出现的名称都自动补全提示、使用QsciAPIs类加入的名称都自动补全提示
     textEdit->setAutoCompletionSource(QsciScintilla::AcsAll);//自动补全,对于所有Ascii字符
     textEdit->setAutoCompletionCaseSensitivity(false);//大小写敏感度
     textEdit->setAutoCompletionThreshold(2);//设置每输入一个字符就会出现自动补全的提示
     //editor->setAutoCompletionReplaceWord(false);//是否用补全的字符串替代光标右边的字符串

     //设置缩进参考线
     textEdit->setIndentationGuides(true);
     //textEdit->setIndentationGuidesBackgroundColor(QColor(Qt::white));
     //textEdit->setIndentationGuidesForegroundColor(QColor(Qt::red));

     //设置光标所在行背景色
     QColor lineColor;
     //lineColor = QColor(Qt::yellow).lighter(150);
     lineColor = QColor(255, 255, 0, 50);
     textEdit->setCaretLineBackgroundColor(lineColor);
     textEdit->setCaretLineVisible(true);

     //设置光标颜色
     if(red < 55) //暗模式，mac下为50
         textEdit->setCaretForegroundColor(QColor(Qt::white));
     textEdit->setCaretWidth(2);


     //自动折叠区域
     textEdit->setMarginType(3, QsciScintilla::SymbolMargin);
     textEdit->setMarginLineNumbers(3, false);
     textEdit->setMarginWidth(3, 15);
     textEdit->setMarginSensitivity(3, true);
     textEdit->setFolding(QsciScintilla::BoxedTreeFoldStyle);//折叠样式
     if(red < 55) //暗模式，mac下为50
     {
         //textEdit->setFoldMarginColors(Qt::gray,Qt::lightGray);//折叠栏颜色
         textEdit->setFoldMarginColors(Qt::gray,Qt::black);
     }

}

void MainWindow::init_treeWidget(QTreeWidget *treeWidgetBack, int w)
{
    //treeWidgetBack = new QTreeWidget();//后台处理数据
    connect(treeWidgetBack, &QTreeWidget::itemClicked, this, &MainWindow::treeWidgetBack_itemClicked);
    treeWidgetBack->setColumnCount(2);
    treeWidgetBack->setMinimumWidth(300);
    treeWidgetBack->setMaximumWidth(w/3);
    treeWidgetBack->setColumnWidth(0 , 500);
    treeWidgetBack->setHeaderItem(new QTreeWidgetItem(QStringList() << "Members" << "Lines"));
    //设置水平滚动条
    treeWidgetBack->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    treeWidgetBack->header()->setStretchLastSection(false);
    treeWidgetBack->setColumnHidden(1 , true);
    treeWidgetBack->setFont(font);
}

void MainWindow::on_chkScope_clicked()
{
    update_ui_tw();
}

void MainWindow::on_chkDevice_clicked()
{
    update_ui_tw();
}

void MainWindow::on_chkMethod_clicked()
{
    update_ui_tw();
}

void MainWindow::on_chkName_clicked()
{
    update_ui_tw();
}
