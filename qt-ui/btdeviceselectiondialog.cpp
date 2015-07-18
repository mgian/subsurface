#include <QShortcut>
#include <QDebug>
#include <QMessageBox>
#include <QMenu>

#include "ui_btdeviceselectiondialog.h"
#include "btdeviceselectiondialog.h"

BtDeviceSelectionDialog::BtDeviceSelectionDialog(QWidget *parent) :
	QDialog(parent),
	localDevice(new QBluetoothLocalDevice),
	ui(new Ui::BtDeviceSelectionDialog)
{
	// Check if Bluetooth is available on this device
	if (!localDevice->isValid()) {
		QMessageBox::warning(this, tr("Warning"),
				     "This should never happen, please contact the Subsurface developers "
				     "and tell them that the Bluetooth download mode doesn't work.");
		return;
	}

	ui->setupUi(this);

	// Quit button callbacks
	QShortcut *quit = new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_Q), this);
	connect(quit, SIGNAL(activated()), this, SLOT(reject()));
	connect(ui->quit, SIGNAL(clicked()), this, SLOT(reject()));

	// Disable the save button because there is no device selected
	ui->save->setEnabled(false);

	connect(ui->discoveredDevicesList, SIGNAL(itemClicked(QListWidgetItem*)),
		this, SLOT(itemClicked(QListWidgetItem*)));

	// Populate the list with local bluetooth devices
	QList<QBluetoothHostInfo> localAvailableDevices = localDevice->allDevices();
	int defaultDeviceIndex = -1;
	int availableDevicesSize = localAvailableDevices.size();

	for (int it = 0; it < availableDevicesSize; it++) {
		QBluetoothHostInfo localAvailableDevice = localAvailableDevices.at(it);
		ui->localSelectedDevice->addItem(localAvailableDevice.name(),
						 QVariant::fromValue(localAvailableDevice.address()));

		if (localDevice->address() == localAvailableDevice.address())
			defaultDeviceIndex = it;
	}

	// Positionate the current index to the default device and register to index changes events
	ui->localSelectedDevice->setCurrentIndex(defaultDeviceIndex);
	connect(ui->localSelectedDevice, SIGNAL(currentIndexChanged(int)),
		this, SLOT(localDeviceChanged(int)));

	// Update the UI information about the local device
	updateLocalDeviceInformation();

	// Intialize the discovery agent
	remoteDeviceDiscoveryAgent = new QBluetoothDeviceDiscoveryAgent();

	connect(remoteDeviceDiscoveryAgent, SIGNAL(deviceDiscovered(QBluetoothDeviceInfo)),
		this, SLOT(addRemoteDevice(QBluetoothDeviceInfo)));
	connect(remoteDeviceDiscoveryAgent, SIGNAL(finished()),
		this, SLOT(remoteDeviceScanFinished()));
}

BtDeviceSelectionDialog::~BtDeviceSelectionDialog()
{
	delete ui;
}

void BtDeviceSelectionDialog::on_changeDeviceState_clicked()
{
	if (localDevice->hostMode() == QBluetoothLocalDevice::HostPoweredOff) {
		ui->dialogStatus->setText("Trying to turn on the local Bluetooth device...");
		localDevice->powerOn();
	} else {
		ui->dialogStatus->setText("Trying to turn off the local Bluetooth device...");
		localDevice->setHostMode(QBluetoothLocalDevice::HostPoweredOff);
	}
}

void BtDeviceSelectionDialog::on_save_clicked()
{
	// Get the selected device. There will be always a selected device if the save button is enabled.
	QListWidgetItem *currentItem = ui->discoveredDevicesList->currentItem();
	QBluetoothDeviceInfo remoteDeviceInfo = currentItem->data(Qt::UserRole).value<QBluetoothDeviceInfo>();

	// Save the selected device
	selectedRemoteDeviceInfo = QSharedPointer<QBluetoothDeviceInfo>(new QBluetoothDeviceInfo(remoteDeviceInfo));

	if (remoteDeviceDiscoveryAgent->isActive()) {
		// Stop the SDP agent if the clear button is pressed and enable the Scan button
		remoteDeviceDiscoveryAgent->stop();
		ui->scan->setEnabled(true);
	}

	// Close the device selection dialog and set the result code to Accepted
	accept();
}

void BtDeviceSelectionDialog::on_clear_clicked()
{
	ui->dialogStatus->setText("Remote devices list was cleaned.");
	ui->discoveredDevicesList->clear();
	ui->save->setEnabled(false);

	if (remoteDeviceDiscoveryAgent->isActive()) {
		// Stop the SDP agent if the clear button is pressed and enable the Scan button
		remoteDeviceDiscoveryAgent->stop();
		ui->scan->setEnabled(true);
	}
}

void BtDeviceSelectionDialog::on_scan_clicked()
{
	ui->dialogStatus->setText("Scanning for remote devices...");
	remoteDeviceDiscoveryAgent->start();
	ui->scan->setEnabled(false);
}

void BtDeviceSelectionDialog::remoteDeviceScanFinished()
{
	ui->dialogStatus->setText("Scanning finished.");
	ui->scan->setEnabled(true);
}

void BtDeviceSelectionDialog::hostModeStateChanged(QBluetoothLocalDevice::HostMode mode)
{
	bool on = !(mode == QBluetoothLocalDevice::HostPoweredOff);

	ui->dialogStatus->setText(QString("The local Bluetooth device was turned %1.")
				  .arg(on? "ON" : "OFF"));
	ui->deviceState->setChecked(on);
	ui->scan->setEnabled(on);
}

void BtDeviceSelectionDialog::addRemoteDevice(const QBluetoothDeviceInfo &remoteDeviceInfo)
{
	QString deviceLable = QString("%1  (%2)").arg(remoteDeviceInfo.name()).arg(remoteDeviceInfo.address().toString());
	QList<QListWidgetItem *> itemsWithSameSignature = ui->discoveredDevicesList->findItems(deviceLable, Qt::MatchStartsWith);

	// Check if the remote device is already in the list
	if (itemsWithSameSignature.empty()) {
		QListWidgetItem *item = new QListWidgetItem(deviceLable);
		QBluetoothLocalDevice::Pairing pairingStatus = localDevice->pairingStatus(remoteDeviceInfo.address());
		item->setData(Qt::UserRole, QVariant::fromValue(remoteDeviceInfo));

		if (pairingStatus == QBluetoothLocalDevice::Paired) {
			item->setText(QString("%1   [State: PAIRED]").arg(item->text()));
			item->setBackgroundColor(QColor(Qt::gray));
		} else if (pairingStatus == QBluetoothLocalDevice::AuthorizedPaired) {
			item->setText(QString("%1   [State: AUTHORIZED_PAIRED]").arg(item->text()));
			item->setBackgroundColor(QColor(Qt::blue));
		} else {
			item->setText(QString("%1   [State: UNPAIRED]").arg(item->text()));
			item->setTextColor(QColor(Qt::black));
		}

		ui->discoveredDevicesList->addItem(item);
	}
}

void BtDeviceSelectionDialog::itemClicked(QListWidgetItem *item)
{
	QBluetoothDeviceInfo remoteDeviceInfo = item->data(Qt::UserRole).value<QBluetoothDeviceInfo>();
	QBluetoothLocalDevice::Pairing pairingStatus = localDevice->pairingStatus(remoteDeviceInfo.address());

	if (pairingStatus == QBluetoothLocalDevice::Unpaired) {
		ui->dialogStatus->setText(QString("The device %1 must be paired in order to be used. Please use the context menu for pairing options.")
					  .arg(remoteDeviceInfo.address().toString()));
		ui->save->setEnabled(false);
	} else {
		ui->dialogStatus->setText(QString("The device %1 can be used for connection. You can press the Save button.")
					  .arg(remoteDeviceInfo.address().toString()));
		ui->save->setEnabled(true);
	}
}

void BtDeviceSelectionDialog::localDeviceChanged(int index)
{
	QBluetoothAddress localDeviceSelectedAddress = ui->localSelectedDevice->itemData(index, Qt::UserRole).value<QBluetoothAddress>();

	// Delete the old localDevice
	if (localDevice)
		delete localDevice;

	// Create a new local device using the selected address
	localDevice = new QBluetoothLocalDevice(localDeviceSelectedAddress);

	// Clear the discovered devices list
	on_clear_clicked();

	// Update the UI information about the local device
	updateLocalDeviceInformation();

	ui->dialogStatus->setText(QString("The local device was changed."));
}

void BtDeviceSelectionDialog::displayPairingMenu(const QPoint &pos)
{
	QMenu menu(this);
	QAction *pairAction = menu.addAction("Pair");
	QAction *removePairAction = menu.addAction("Remove Pairing");
	QAction *chosenAction = menu.exec(ui->discoveredDevicesList->viewport()->mapToGlobal(pos));
	QListWidgetItem *currentItem = ui->discoveredDevicesList->currentItem();
	QBluetoothDeviceInfo currentRemoteDeviceInfo = currentItem->data(Qt::UserRole).value<QBluetoothDeviceInfo>();
	QBluetoothLocalDevice::Pairing pairingStatus = localDevice->pairingStatus(currentRemoteDeviceInfo.address());

	//TODO: disable the actions
	if (pairingStatus == QBluetoothLocalDevice::Unpaired) {
		pairAction->setEnabled(true);
		removePairAction->setEnabled(false);
	} else {
		pairAction->setEnabled(false);
		removePairAction->setEnabled(true);
	}

	if (chosenAction == pairAction) {
		ui->dialogStatus->setText(QString("Trying to pair device %1")
					  .arg(currentRemoteDeviceInfo.address().toString()));
		localDevice->requestPairing(currentRemoteDeviceInfo.address(), QBluetoothLocalDevice::Paired);
	} else if (chosenAction == removePairAction) {
		ui->dialogStatus->setText(QString("Trying to unpair device %1")
					  .arg(currentRemoteDeviceInfo.address().toString()));
		localDevice->requestPairing(currentRemoteDeviceInfo.address(), QBluetoothLocalDevice::Unpaired);
	}
}

void BtDeviceSelectionDialog::pairingFinished(const QBluetoothAddress &address, QBluetoothLocalDevice::Pairing pairing)
{
	QString remoteDeviceStringAddress = address.toString();
	QList<QListWidgetItem *> items = ui->discoveredDevicesList->findItems(remoteDeviceStringAddress, Qt::MatchContains);

	if (pairing == QBluetoothLocalDevice::Paired || pairing == QBluetoothLocalDevice::Paired ) {
		ui->dialogStatus->setText(QString("Device %1 was paired.")
					  .arg(remoteDeviceStringAddress));

		for (int i = 0; i < items.count(); ++i) {
			QListWidgetItem *item = items.at(i);

			item->setText(QString("%1   [State: PAIRED]").arg(remoteDeviceStringAddress));
			item->setBackgroundColor(QColor(Qt::gray));
		}

		QListWidgetItem *currentItem = ui->discoveredDevicesList->currentItem();

		if (currentItem != NULL && currentItem->text().contains(remoteDeviceStringAddress, Qt::CaseInsensitive)) {
			ui->dialogStatus->setText(QString("The device %1 can now be used for connection. You can press the Save button.")
						  .arg(remoteDeviceStringAddress));
			ui->save->setEnabled(true);
		}
	} else {
		ui->dialogStatus->setText(QString("Device %1 was unpaired.")
					  .arg(remoteDeviceStringAddress));

		for (int i = 0; i < items.count(); ++i) {
			QListWidgetItem *item = items.at(i);

			item->setText(QString("%1   [State: UNPAIRED]").arg(remoteDeviceStringAddress));
			item->setBackgroundColor(QColor(Qt::white));
		}

		QListWidgetItem *currentItem = ui->discoveredDevicesList->currentItem();

		if (currentItem != NULL && currentItem->text().contains(remoteDeviceStringAddress, Qt::CaseInsensitive)) {
			ui->dialogStatus->setText(QString("The device %1 must be paired in order to be used. Please use the context menu for pairing options.")
						  .arg(remoteDeviceStringAddress));
			ui->save->setEnabled(false);
		}
	}
}

void BtDeviceSelectionDialog::error(QBluetoothLocalDevice::Error error)
{
	ui->dialogStatus->setText(QString("Local device error: %1.")
				  .arg((error == QBluetoothLocalDevice::PairingError)? "Pairing error" : "Unknown error"));
}

QString BtDeviceSelectionDialog::getSelectedDeviceAddress()
{
	if (selectedRemoteDeviceInfo) {
		return selectedRemoteDeviceInfo.data()->address().toString();
	}

	return QString();
}

QString BtDeviceSelectionDialog::getSelectedDeviceName()
{
	if (selectedRemoteDeviceInfo) {
		return selectedRemoteDeviceInfo.data()->name();
	}

	return QString();
}

void BtDeviceSelectionDialog::updateLocalDeviceInformation()
{
	// Set UI information about the local device
	ui->deviceAddress->setText(localDevice->address().toString());
	ui->deviceName->setText(localDevice->name());

	connect(localDevice, SIGNAL(hostModeStateChanged(QBluetoothLocalDevice::HostMode)),
		this, SLOT(hostModeStateChanged(QBluetoothLocalDevice::HostMode)));

	// Initialize the state of the local device and activate/deactive the scan button
	hostModeStateChanged(localDevice->hostMode());

	// Add context menu for devices to be able to pair them
	ui->discoveredDevicesList->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(ui->discoveredDevicesList, SIGNAL(customContextMenuRequested(QPoint)),
		this, SLOT(displayPairingMenu(QPoint)));
	connect(localDevice, SIGNAL(pairingFinished(QBluetoothAddress, QBluetoothLocalDevice::Pairing)),
		this, SLOT(pairingFinished(QBluetoothAddress, QBluetoothLocalDevice::Pairing)));

	connect(localDevice, SIGNAL(error(QBluetoothLocalDevice::Error)),
		this, SLOT(error(QBluetoothLocalDevice::Error)));
}
