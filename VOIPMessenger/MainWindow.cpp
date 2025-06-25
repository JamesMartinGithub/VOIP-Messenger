#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "time.h"
#include <qstringfwd.h>
#include <iostream>

MainWindow::MainWindow(Controller* pController, QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow),
      controller(pController) {
    ui->setupUi(this);
    setFixedSize(size());
    setWindowFlags(Qt::Window | Qt::MSWindowsFixedSizeDialogHint);
    ui->messageInput->installEventFilter(this);
    // Initialise friend list from file
    for (std::string name : controller->GetFriends()) {
        friends.insert(name);
        ui->friendList->insertItem(0, QString::fromStdString(name));
    }
}

MainWindow::~MainWindow() {
    delete ui;
}

void MainWindow::DisplayInputDevices(std::list<std::wstring> devices) {
    if (devices.size() > 0) {
        for (std::wstring name : devices) {
            ui->micDropdown->addItem(QString::fromStdWString(name));
        }
    }
}

void MainWindow::DisplayConnectionState(std::string message, Controller::ConnectionType connection, bool isTCP) {
    if (isTCP) {
        ui->callButton->setChecked(false);
        switch (connection) {
            case Controller::ConnectionType::DISCONNECTED:
                currentConnectionState = Controller::ConnectionType::DISCONNECTED;
                ui->connectButton->setText("Connect");
                ui->connectButton->setChecked(false);
                ui->connectButton->setEnabled(true);
                ui->serverButton->setEnabled(true);
                ui->ipInput->setEnabled(true);
                ui->messageInput->setEnabled(false);
                ui->callButton->setEnabled(false);
                break;
            case Controller::ConnectionType::CONNECTING:
                currentConnectionState = Controller::ConnectionType::CONNECTING;
                if (ui->serverButton->isChecked()) {
                    ui->connectButton->setText("Cancel");
                }
                ui->connectButton->setEnabled(true);
                break;
            case Controller::ConnectionType::CONNECTED:
                currentConnectionState = Controller::ConnectionType::CONNECTED;
                ui->connectButton->setText("Disconnect");
                ui->connectButton->setChecked(true);
                ui->connectButton->setEnabled(true);
                ui->serverButton->setEnabled(false);
                ui->messageInput->setEnabled(true);
                ui->callButton->setEnabled(true);
                break;
        }
    } else {
        switch (connection) {
        case Controller::ConnectionType::DISCONNECTED:
            ui->callButton->setChecked(false);
            ui->callButton->setEnabled(true);
            ui->micDropdown->setEnabled(true);
            break;
        case Controller::ConnectionType::CONNECTING: // Used for tcp connection shutdown
            ui->callButton->setEnabled(false);
            ui->micDropdown->setEnabled(true);
            break;
        case Controller::ConnectionType::CONNECTED:
            ui->callButton->setChecked(true);
            ui->callButton->setEnabled(true);
            ui->micDropdown->setEnabled(false);
            break;
        }
    }
    QString displayMessage = "<span style=\"color:#888888;font-style:italic;\">";
    displayMessage.append(message);
    displayMessage.append("</span>");
    ui->messageDisplay->append(displayMessage);
}

bool MainWindow::eventFilter(QObject* object, QEvent* event) {
    if (object == ui->messageInput && event->type() == QEvent::KeyPress && static_cast<QKeyEvent *>(event)->key() == Qt::Key_Return) {
        char time[128];
        _strtime_s(time, 128);
        QString netMessage = "[";
        netMessage.append(time);
        netMessage.append("]    ");
        netMessage.append(ui->messageInput->toPlainText());
        QString viewMessage = "";
        // Send message to network object
        if (controller->SendData(netMessage.toStdString().c_str(), netMessage.length())) {
            viewMessage.append("<span style=\"color:#00BB00;\">[");
            viewMessage.append(time);
            viewMessage.append("]</span>    ");
            viewMessage.append(ui->messageInput->toPlainText());
        } else {
            viewMessage.append("<span style=\"color:#82BA82;\">[");
            viewMessage.append(time);
            viewMessage.append("]</span>    <span style=\"color:#888888;\">");
            viewMessage.append(ui->messageInput->toPlainText());
            viewMessage.append("</span>");
            
        }
        ui->messageDisplay->append(viewMessage + "\n");
        //ui->messageDisplay->append("<img src = \"W:/Pictures/djathome.PNG\"/>");
        ui->messageInput->setPlainText("");
        // Save message to file
        if (loadedFriend != "") {
            controller->SaveMessage(loadedFriend, netMessage.toStdString(), true);
        }
        return true;
    } else {
        return false;
    }
}

void MainWindow::DisplayMessage(std::string message) {
    QString qMessage = QString::fromStdString(message);
    ui->messageDisplay->append("<span style=\"color:#BB0000;\">" + qMessage.sliced(0, 10) + "</span>" + qMessage.sliced(10) + "\n");
    // Save message to file
    if (loadedFriend != "") {
        controller->SaveMessage(loadedFriend, message, false);
    }
}

void MainWindow::on_ipInput_editingFinished() {

}

void MainWindow::on_nameInput_editingFinished() {
    ui->messageDisplay->clear();
}

void MainWindow::on_connectButton_clicked() {
    auto adjustWidgets = [&]() {
        ui->ipInput->setEnabled(false);
        ui->connectButton->setChecked(false);
        ui->connectButton->setEnabled(false);
        ui->serverButton->setEnabled(false);
    };

    if (currentConnectionState == Controller::ConnectionType::CONNECTING || currentConnectionState == Controller::ConnectionType::CONNECTED) {
        // Cancel connection
        controller->Disconnect();
    } else {
        // Start connecting
        if (controller->IsServer()) {
            adjustWidgets();
            controller->Connect();
        } else {
            QString ipText = ui->ipInput->text();
            QStringList parts = ipText.split(':');
            if (parts.count() == 2 && parts[1].length() <= 5) {
                QString ip = parts[0];
                QString port = parts[1];
                adjustWidgets();
                controller->Connect(ip.toStdString().c_str(), port.toStdString().c_str());
            } else {
                ui->connectButton->setChecked(false);
            }
        }
    }
}

void MainWindow::on_serverButton_clicked(){
    if (controller->ToggleServer()) {
        ui->serverButton->setChecked(true);
        ui->ipInput->setText("");
        ui->ipInput->setEnabled(false);
    } else {
        ui->serverButton->setChecked(false);
        ui->ipInput->setEnabled(true);
    }
}

void MainWindow::on_messageInput_textChanged() {
    if (ui->messageInput->toPlainText().length() > 500) {
        ui->messageInput->undo();
    }
}

void MainWindow::on_friendList_itemPressed(QListWidgetItem* item) {
    QString itemText = item->text();
    if (itemText == "Add Friend") {
        std::string inputName = ui->nameInput->text().toStdString();
        // Check if friend already exists or name is empty
        if (!friends.contains(inputName) && inputName != "") {
            ui->friendList->clearSelection();
            friends.insert(inputName);
            // Add friend to friend list
            ui->friendList->insertItem(0, ui->nameInput->text());
            ui->friendList->setCurrentRow(0);
            // Store friend data
            controller->ModifyFriend(inputName, true);
            loadedFriend = inputName;
        }
    } else if (itemText == "Remove Friend") {
        ui->friendList->clearSelection();
        // Check if friend data is loaded
        if (loadedFriend != "") {
            ui->messageDisplay->clear();
            // Remove friend from friend list
            ui->friendList->takeItem(ui->friendList->row(ui->friendList->findItems(QString::fromStdString(loadedFriend), Qt::MatchFlag::MatchExactly).first()));
            // Delete friend data
            controller->ModifyFriend(loadedFriend, false);
            friends.erase(loadedFriend);
            loadedFriend = "";
            ui->nameInput->setText("");
        }
    } else {
        // Check if friend messages not already loaded
        if (itemText != loadedFriend) {
            std::vector<std::string> messages = controller->GetFriendMessages(itemText.toStdString());
            ui->messageDisplay->clear();
            // Add messages to display and set colour of timestamps depending on who sender was
            for (std::string message : messages) {
                QString qMessage = QString::fromStdString(message.substr(1, message.length()));
                ui->messageDisplay->append((message[0] == 'S' ? "<span style=\"color:#82BA82;\">" : "<span style=\"color:#BA8282;\">") + qMessage.sliced(0, 10) + "</span><span style=\"color:#888888;\">" + qMessage.sliced(10) + "</span>\n");
            }
            loadedFriend = itemText.toStdString();
            ui->nameInput->setText(itemText);
        }
    }
}

void MainWindow::on_callButton_clicked(bool checked) {
    std::cout << (checked ? "CHECKED = TRUE" : "CHECKED = FALSE") << '\n';
    if (!checked) {
        // Stop call
        controller->DisconnectCall();
    } else {
        // Start call
        ui->callButton->setEnabled(false);
        ui->micDropdown->setEnabled(false);
        controller->ConnectCall();
    }
}

void MainWindow::on_micDropdown_currentIndexChanged(int index) {
    controller->SetInputDevice(index);
}
