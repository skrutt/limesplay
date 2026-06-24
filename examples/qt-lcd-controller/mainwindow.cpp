#include "mainwindow.h"
#include "ui_mainwindow.h"


//void sendfunc
MainWindow * mainw_ptr;

void set_ptr(MainWindow * ptr)
{
    mainw_ptr = ptr;
}

void extradatacallback1(uint8_t byte)
{
    mainw_ptr->output_byte(byte);
//    ui->outputplainTextEdit->insertPlainText(QString::fromLatin1((char*)pac->data, pac->length));
}


void extradatacallback(uint8_t byte)
{
    qDebug().noquote().nospace() << /*(char)*/byte;
}
void extra_sendfunc(const void *data, uint16_t len)
{
    mainw_ptr->sendfunc((const uint8_t*)data, len);
}

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    petcol1(extra_sendfunc, extradatacallback1)

{
    ui->setupUi(this);

    QPalette pal = palette();
    // set black background
    pal.setColor(QPalette::Base, Qt::black);
    pal.setColor(QPalette::Text, Qt::green);
    ui->outputplainTextEdit->setPalette(pal);

    //connect signals
    connect(&serialPort, &QSerialPort::bytesWritten,
            this, &MainWindow::handleBytesWritten);
    connect(&serialPort, &QSerialPort::readyRead,
            this, &MainWindow::handleReadyRead);
    connect(&serialPort, &QSerialPort::errorOccurred,
            this, &MainWindow::handleError);

    set_ptr(this);

    //set com port
    const QString serialPortName = "/dev/ttyUSB0";
    serialPort.setPortName(serialPortName);
    serialPort.setBaudRate(QSerialPort::Baud115200);

    if (!serialPort.open(QIODevice::ReadWrite))
    {
        ui->outputplainTextEdit->appendPlainText("Failed to open port " + serialPortName);
    }

    //setup timer
    apptimer.setInterval(1000);
    connect(&apptimer, &QTimer::timeout, this, &MainWindow::app_timeout);
    apptimer.start();

    //Call to set enable/disable of ui elements
    on_sendWindowTitleCheckBox_stateChanged(0);

//    petcol1.set_extra_data_callback(extradatacallback1);

//    serialPort.write()
}

MainWindow::~MainWindow()
{
    serialPort.close();
    delete ui;
}

void MainWindow::output_byte(uint8_t byte)
{
    ui->outputplainTextEdit->insertPlainText(QString::fromLatin1((char*)&byte, 1));
}

void MainWindow::sendfunc(const uint8_t *data, uint16_t len)
{
    serialPort.write((const char*)data, len);
//    serialPort.waitForBytesWritten(1);
}

typedef struct {
    uint16_t type;
    float value;
} com_float_type_t;

void MainWindow::handleReadyRead()
{
    QByteArray derp = serialPort.readAll();
//    derp.replace("\0", "");
    while(derp.size())
    {
        packet_recieved *pac = petcol1.recv_byte_input((uint8_t)derp.front());
//        for(int i = 0; i < pac->length; i++)
//        {
        if(pac)
        {
            if(((com_float_type_t*)pac->data)->type == 1)
            {
                float temp;
                memcpy(&temp, pac->data + 2, 4);
//                ui->outputplainTextEdit->insertPlainText(QString("\nHere is the current temp: "));
                ui->tempLineEdit->setText(QString::number(temp, 'g', 3));
//                ui->outputplainTextEdit->insertPlainText(QString("\n\n"));
            }
            else
            {
                //we got an unknown packet
                ui->outputplainTextEdit->insertPlainText(QString::fromLatin1((char*)pac->data, pac->length));
            }
        }

        derp = derp.mid(1); //Chop the first one off
    }
    ui->outputplainTextEdit->insertPlainText(derp); //append any remaining chars
}

void MainWindow::handleBytesWritten(qint64 bytes)
{
//    ui->outputplainTextEdit->insertPlainText("We sent data, number: " + QString::number(bytes));
}

void MainWindow::handleError(QSerialPort::SerialPortError error)
{
    ui->outputplainTextEdit->insertPlainText("QSerialPort: We got error: " + serialPort.errorString());
}


//timeout, update text on display
void MainWindow::app_timeout()
{
    QString firstLine;
    QString secondLine;

    if(ui->sendWindowTitleCheckBox->isChecked())
    {
        //Get name of currently active window
        QProcess process(this);
        process.setProgram("xdotool");
        process.setArguments(QStringList() << "getwindowfocus" << "getwindowname");
        process.start();
        while(process.state() != QProcess::NotRunning)
            qApp->processEvents();
        secondLine = firstLine = process.readAll().replace('\n', "");
        firstLine.resize(20, ' ');  //Resize, we dont want to scroll first line
        secondLine.remove(0, 20);
    }
    else
    {
        //user disabled windowtitle display, get text from line edit instead
        firstLine = ui->firstLineEdit->text();
        secondLine = ui->secondLineEdit->text();
    }

    if(ui->scrollCheckBox->isChecked())
    {
        static int scroll1pos = 0, scroll2pos = 0;

        int scroll1 = firstLine.length();
        if(scroll1 > 20)
        {
            scroll1 -= 19; //one less since 21%1 = 0
            firstLine.remove(0, scroll1pos % scroll1);
            scroll1pos++;
        }
        else
            scroll1pos = 0;

        int scroll2 = secondLine.length();
        if(scroll2 > 20)
        {
            scroll2 -= 19; //one less since 21%1 = 0
            secondLine.remove(0, scroll2pos % scroll2);
            scroll2pos++;
        }
        else
            scroll2pos = 0;
    }

    QString stringToSend;
    stringToSend = firstLine;
    stringToSend.resize(20, ' ');
    stringToSend += secondLine;
    stringToSend.resize(40, ' ');   //Pad to length

    QByteArray bytesToSend = stringToSend.toLatin1();
//    def scandinavianSupport(text):
//        textlist = list(text)   #make a list
//        text=''
//        for c in range(len(textlist)):
//            if textlist[c]=='å':
//                text+=chr(134)
//            elif textlist[c]=='Å':
//                text+=chr(143)
//            elif textlist[c]=='ä':
//                text+=chr(225) # new
//            elif textlist[c]=='Ä':     #change letters to right machinecode
//                text+=chr(142)
//            elif textlist[c]=='ö':
//                text+=chr(239)  # new
//            elif textlist[c]=='Ö':
//                text+=chr(153)
//            else:
//                text+=repr(textlist[c])[2]
//        return text #return new string

    bytesToSend.replace(229, 1); // å    //Added input numbers, need to create outputs in firmware... or if we scan the characters in the display?
    bytesToSend.replace(197, 2); // Å
    bytesToSend.replace(228, 0b11100001); // ä
    bytesToSend.replace(196, 3); // Ä
    bytesToSend.replace(246, 0b11101111); // ö
    bytesToSend.replace(214, 4); // Ö    // These do not exist as characters in rom, we need to use them from RAM, and define them in FW first
    bytesToSend.replace("\n", " "); //safety, cannot display
    bytesToSend.prepend("\01");

    petcol1.sendFunc(bytesToSend.data(), 40 + 1);
}
//Set disablement of ui elements in callback, snappy ui... function is at void MainWindow::app_timeout()
void MainWindow::on_sendWindowTitleCheckBox_stateChanged(int arg1)
{
    if(ui->sendWindowTitleCheckBox->isChecked())
    {
        ui->firstLineEdit->setEnabled(false);
        ui->secondLineEdit->setEnabled(false);
    }
    else
    {
        ui->firstLineEdit->setEnabled(true);
        ui->secondLineEdit->setEnabled(true);
    }
}

void MainWindow::update_led_color()
{
   // qDebug() << sizeof(led_color);

    uint8_t data[4] = {2, led_color.red, led_color.green, led_color.blue};

    //Send data
    petcol1.sendFunc(data, 4);

    //Set color indicator
    ui->ledColorButton->setAutoFillBackground(true);
    QColor showLEDColor;
    showLEDColor.setRgb(led_color.red, led_color.green, led_color.blue);
    QPalette pal = palette();
    pal.setColor(QPalette::Button, showLEDColor);
    ui->ledColorButton->setPalette(pal);

}

void MainWindow::on_redhorizontalSlider_valueChanged(int value)
{
    led_color.red = value;
    update_led_color();
}

void MainWindow::on_greenhorizontalSlider_valueChanged(int value)
{
    led_color.green = value;
    update_led_color();
}

void MainWindow::on_bluehorizontalSlider_valueChanged(int value)
{
    led_color.blue = value;
    update_led_color();
}
