/*
 * Copyright (C) 2016 Luiz Carlos Vieira (http://www.luiz.vieira.nom.br)
 *               2017 Philipp Werner (http://philipp-werner.info)
 *
 * This file is part of FLAT.
 *
 * FLAT is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * FLAT is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "aboutwindow.h"
#include "facefitconfig.h"
#include "utils.h"
#include "application.h"

#include <vector>

#include <QApplication>
#include <QImage>
#include <QFileDialog>
#include <QDesktopServices>
#include <QAction>
#include <QMessageBox>
#include <QTemporaryFile>
#include <QMenu>

using namespace std;

// +-----------------------------------------------------------
ft::MainWindow::MainWindow(QWidget *pParent) :
    QMainWindow(pParent),
    ui(new Ui::MainWindow)
{
	// Setup the UI
    ui->setupUi(this);
	QPalette oROPalette = ui->textFileName->palette();
	oROPalette.setColor(QPalette::Base, oROPalette.midlight().color());
	ui->textFileName->setPalette(oROPalette);

    setWindowState(Qt::WindowMaximized);
    m_pAbout = NULL;

    setWindowIcon(QIcon(":/icons/fat"));
	ui->tabWidget->setAutoFillBackground(true);
	ui->tabWidget->setBackgroundRole(QPalette::Midlight);

	// Setup the TAB changing events
	connect(ui->tabWidget, SIGNAL(tabCloseRequested(int)), this, SLOT(on_tabCloseRequested(int)));
	connect(ui->tabWidget, SIGNAL(currentChanged(int)), this, SLOT(on_tabChanged(int)));

	// Add the view dropdown button manually (because Qt designer does not allow it...)
	m_pViewButton = new QMenu(ui->imagesToolbar);
	ui->imagesToolbar->addSeparator();
	ui->imagesToolbar->addAction(m_pViewButton->menuAction());

	QAction *pViewDetails = new QAction(QIcon(":/icons/viewdetails"), tr("&Details"), this);
	m_pViewButton->addAction(pViewDetails);
	QAction *pViewIcons = new QAction(QIcon(":/icons/viewicons"), tr("&Icons"), this);
	m_pViewButton->addAction(pViewIcons);

	QSignalMapper *pMap = new QSignalMapper(ui->imagesToolbar);
	connect(pViewDetails, SIGNAL(triggered()), pMap, SLOT(map()));
	connect(pViewIcons, SIGNAL(triggered()), pMap, SLOT(map()));
	pMap->setMapping(pViewDetails, QString("details"));
	pMap->setMapping(pViewIcons, QString("icons"));

	connect(pMap, SIGNAL(mapped(QString)), this, SLOT(setImageListView(QString)));
	connect(m_pViewButton->menuAction(), SIGNAL(triggered()), this, SLOT(toggleImageListView()));

	m_pViewButton->setIcon(QIcon(":/icons/viewicons")); // By default display the image thumbnails
	ui->treeImages->setVisible(false);

	// Default path for file dialogs is the standard documents path
	m_sLastPathUsed = QDir::toNativeSeparators(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)) + QDir::separator();

	// Default path for the face-fit utility
	m_sFaceFitPath = "";
	m_oFitProcess = new QProcess(this);
	connect(m_oFitProcess, SIGNAL(error(QProcess::ProcessError)), this, SLOT(onFitError(::ProcessError)));
	connect(m_oFitProcess, SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(onFitFinished(int, QProcess::ExitStatus)));

	// Add the action shortcuts to the tooltips (in order to make it easier for the user to know they exist)
	// P.S.: I wonder why doesn't Qt do that automatically... :)
	QObjectList lsObjects = children();
	QAction *pAction;
	for(int i = 0; i < lsObjects.size(); i++)
	{
		pAction = qobject_cast<QAction*>(lsObjects.at(i));
		if(pAction && !pAction->shortcut().isEmpty())
			pAction->setToolTip(QString("%1 (%2)").arg(pAction->toolTip()).arg(pAction->shortcut().toString()));
	}

	// Connect the zoom slider
	connect(ui->zoomSlider, SIGNAL(valueChanged(int)), this, SLOT(onSliderValueChanged(int)));

#ifndef DLIB_INTEGRATION
	ui->menuDlib->setEnabled(false);
#endif
}

// +-----------------------------------------------------------
ft::MainWindow::~MainWindow()
{
	// Save the current window state and geometry
	QSettings oSettings;
	oSettings.setValue("geometry", saveGeometry());
	oSettings.setValue("windowState", saveState());
	oSettings.setValue("lastPathUsed", m_sLastPathUsed);
	oSettings.setValue("faceFitPath", m_sFaceFitPath);
	oSettings.setValue("dlibFaceDetModelFilename", m_sDlibFaceDetModelFilename);
	oSettings.setValue("dlibLandmarkLocModelFilename", m_sDlibLandmarkLocModelFilename);

    if(m_pAbout)
        delete m_pAbout;
	if(m_pViewButton)
		delete m_pViewButton;
	if (m_oFitProcess)
		delete m_oFitProcess;
    delete ui;
}

// +-----------------------------------------------------------
void ft::MainWindow::closeEvent(QCloseEvent *pEvent)
{
	QList<ChildWindow*> lModified;
	ChildWindow *pChild;

	// Get the list of modified child windows
	for(int i = 0; i < ui->tabWidget->count(); i++)
	{
		pChild = (ChildWindow*) ui->tabWidget->widget(i);
		if(pChild->isWindowModified())
			lModified.append(pChild);
	}

	// Ask for saving the modifiled child windows
	if(lModified.size() > 0)
	{
		QString sMsg;
		if(lModified.size() == 1)
		{
			pChild = lModified[0];
			sMsg = tr("There are pending changes in the face annotation dataset named [%1]. Do you wish to save it before closing?").arg(QFileInfo(pChild->windowFilePath()).baseName());
		}
		else
			sMsg = tr("There are %1 face annotation datasets opened with pending changes. Do you wish to save them before closing?").arg(lModified.size());

		QMessageBox::StandardButton oResp = QMessageBox::question(this, tr("Pending changes"), sMsg, QMessageBox::Yes|QMessageBox::No|QMessageBox::Cancel);

		// Do not close the tab if the user has chosen "cancel" or if she has chosen "yes" but then
		// cancelled the file save dialog in any of the modified tab
		if(oResp == QMessageBox::Cancel)
		{
			pEvent->ignore();
			return;
		}
		else if(oResp == QMessageBox::Yes)
		{
			for(int i = 0; i < lModified.size(); i++)
			{
				pChild = lModified[i];
				ui->tabWidget->setCurrentWidget(pChild);
				if(!saveCurrentFile())
				{
					pEvent->ignore();
					return;
				}
			}
		}
	}

	pEvent->accept();
}

// +-----------------------------------------------------------
void ft::MainWindow::showEvent(QShowEvent *pEvent)
{
	// Restore the previous window state and geometry
	QSettings oSettings;
	restoreState(oSettings.value("windowState").toByteArray());
	restoreGeometry(oSettings.value("geometry").toByteArray());
	QVariant vValue = oSettings.value("lastPathUsed");
	if (vValue.isValid())
		m_sLastPathUsed = vValue.toString();
	vValue = oSettings.value("faceFitPath");
	if (vValue.isValid())
		m_sFaceFitPath = vValue.toString();
	vValue = oSettings.value("dlibFaceDetModelFilename");
	if (vValue.isValid())
		m_sDlibFaceDetModelFilename = vValue.toString();
	vValue = oSettings.value("dlibLandmarkLocModelFilename");
	if (vValue.isValid())
		m_sDlibLandmarkLocModelFilename = vValue.toString();

	// Update UI elements
	updateUI();
	ui->actionShowImagesList->setChecked(ui->dockImages->isVisible());
	ui->actionShowImageProperties->setChecked(ui->dockProperties->isVisible());

	pEvent->accept();
}

// +-----------------------------------------------------------
void ft::MainWindow::on_actionNew_triggered()
{
    createChildWindow();
}

// +-----------------------------------------------------------
void ft::MainWindow::on_actionOpen_triggered()
{
    QString sFile = QFileDialog::getOpenFileName(this, tr("Open face annotation dataset..."), m_sLastPathUsed, tr("Face Annotation Dataset files (*.fad);; All files (*.*)"));
	openFile(sFile);
}

// +-----------------------------------------------------------
void ft::MainWindow::on_actionSave_triggered()
{
	saveCurrentFile();
}

// +-----------------------------------------------------------
void ft::MainWindow::on_actionSaveAs_triggered()
{
	saveCurrentFile(true);
}

// +-----------------------------------------------------------
bool ft::MainWindow::openFile(const QString & sFile_)
{
	if (!sFile_.length())
		return false;
	
	m_sLastPathUsed = QFileInfo(sFile_).absolutePath();
	QString sFile = QDir::toNativeSeparators(sFile_);
	int iPage = getFilePageIndex(sFile);
	if (iPage != -1)
	{
		ui->tabWidget->setCurrentIndex(iPage);
		showStatusMessage(QString(tr("The face annotation dataset [%1] is already opened in the editor.")).arg(Utils::shortenPath(sFile)));
	}
	else
	{
		ChildWindow *pChild = createChildWindow(sFile, false);

		QString sMsg;
		if (!pChild->loadFromFile(sFile, sMsg))
		{
			destroyChildWindow(pChild);
			QMessageBox::warning(this, tr("Fail to load the face annotation dataset"), tr("It was not possible to open the face annotation dataset:\n%1").arg(sMsg), QMessageBox::Ok);
			return false;
		}

		if (pChild->dataModel()->rowCount() > 0)
			pChild->selectionModel()->setCurrentIndex(pChild->dataModel()->index(0, 0), QItemSelectionModel::Select);
		updateUI();
	}

	return true;
}

// +-----------------------------------------------------------
bool ft::MainWindow::saveCurrentFile(bool bAskForFileName)
{
	ChildWindow *pChild = (ChildWindow*) ui->tabWidget->currentWidget();
	if(!pChild || (!bAskForFileName && !pChild->isWindowModified()))
		return false;

	if(bAskForFileName)
	{
		QString sFileName = QFileDialog::getSaveFileName(this, tr("Save face annotation dataset..."), windowFilePath(), tr("Face Annotation Dataset files (*.fad);; All files (*.*)"));
		if(sFileName.length())
		{
			QString sMsg;
			if(!pChild->saveToFile(sFileName, sMsg))
			{
				QMessageBox::warning(this, tr("Fail to save the face annotation dataset"), tr("It was not possible to save the face annotation dataset:\n%1").arg(sMsg), QMessageBox::Ok);
				return false;
			}

			m_sLastPathUsed = QFileInfo(sFileName).absolutePath();
			return true;
		}
		else
			return false;
	}
	else
	{
		// Force the user to chose a file name if the dataset has not yet been saved
		if(pChild->property("new").toBool())
			return saveCurrentFile(true);
		else
		{
			QString sMsg;
			if(!pChild->save(sMsg))
			{
				QMessageBox::warning(this, tr("Fail to save the face annotation dataset"), tr("It was not possible to save the face annotation dataset:\n%1").arg(sMsg), QMessageBox::Ok);
				return false;
			}

			return true;
		}
	}
}

// +-----------------------------------------------------------
void ft::MainWindow::on_actionImportImageDirPts_triggered()
{
	ChildWindow *pChild = (ChildWindow*)ui->tabWidget->currentWidget();
	if (!pChild)
		return;

	QString path = QFileDialog::getExistingDirectory(this, tr("Select image directory"),
		"", QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

	if (path.isEmpty())
		return;

	// find image files
	QDir dir(path);
	QStringList filters;
	filters << "*.jpg" << "*.png";
	QStringList lsFiles = dir.entryList(filters, QDir::Files);

	if (lsFiles.size())
	{
		// add images
		for (int i = 0; i < lsFiles.size(); ++i)
			lsFiles[i] = path + '/' + lsFiles[i];
		pChild->dataModel()->addImages(lsFiles);

		// load pts files
		FaceDataset * ds = pChild->dataModel()->getFaceDataset();
		int n_feat = -1;
		QString err_message;
		for (int i = 0; i < pChild->dataModel()->rowCount(); ++i)
		{
			//QModelIndex midx = pChild->dataModel()->index(i, 0);
			//QString sImageFile = pChild->dataModel()->data(midx, Qt::DisplayRole).toString();
			FaceImage * img = ds->getImage(i);
			if (!img)
				continue;
			if (img->loadPtsFile(err_message, n_feat))
			{
				if (n_feat == -1)
				{
					n_feat = img->getFeatures().size();
					ds->setNumFeatures(n_feat);
				}
			}
			else
			{
				if (QMessageBox::critical(this, tr("Error"), tr("%1\nContinue to read PTS files of next images?").arg(err_message),
					QMessageBox::Ok | QMessageBox::Cancel) == QMessageBox::Cancel)
					break;
			}
		}
		
		m_sLastPathUsed = QFileInfo(lsFiles[0]).absolutePath();
		if (!pChild->selectionModel()->currentIndex().isValid())
			pChild->selectionModel()->setCurrentIndex(pChild->dataModel()->index(0, 0), QItemSelectionModel::Select);
	}
	else
	{
		QMessageBox::warning(this, "Warning", "No images were found in the selected directory.");
	}
}

// +-----------------------------------------------------------
void ft::MainWindow::on_actionExportPts_triggered()
{
	ChildWindow *pChild = (ChildWindow*)ui->tabWidget->currentWidget();
	if (!pChild)
		return;

	FaceDataset * ds = pChild->dataModel()->getFaceDataset();
	for (int i = 0; i < pChild->dataModel()->rowCount(); ++i)
	{
		FaceImage * img = ds->getImage(i);
		if (!img)
			continue;
		if (!img->savePtsFile(true))
		{
			if (QMessageBox::critical(this, tr("Error"), tr("Failed to save PTS file for '%1'\nContinue to save PTS files?").arg(img->fileName()),
				QMessageBox::Ok | QMessageBox::Cancel) == QMessageBox::Cancel)
				break;
		}
	}
}

// +-----------------------------------------------------------
void ft::MainWindow::on_actionExit_triggered()
{
    QApplication::exit(0);
}

// +-----------------------------------------------------------
void ft::MainWindow::on_actionConfigure_triggered()
{
	FaceFitConfig oConfig;
	if (oConfig.exec() == FaceFitConfig::Accepted)
		m_sFaceFitPath = oConfig.getFaceFitPath();
}

// +-----------------------------------------------------------
void ft::MainWindow::on_actionFitLandmarks_triggered()
{
	// Only continue if the face-fit utilility is properly configured
	if (m_sFaceFitPath.length() < 0 || !QFileInfo(m_sFaceFitPath).exists())
	{
		QMessageBox::critical(this, tr("Error running face-fit utility"), tr("The face-fit utility executable could not be executed. Please check its configuration."), QMessageBox::Ok);
		return;
	}

	// Get the selected face annotation dataset
	ChildWindow *pChild = (ChildWindow*)ui->tabWidget->currentWidget();
	if (!pChild) // Sanity check
		return;

	// Get the path of the selected image in the dataset
	QModelIndex oSelectedImage = pChild->selectionModel()->currentIndex();
	QModelIndex oIdx = pChild->dataModel()->index(oSelectedImage.row(), 1);
	QString sImageFile = pChild->dataModel()->data(oIdx, Qt::DisplayRole).toString();

	// Build the arguments for the face-fit utility
	QTemporaryFile oTemp("face-fit-results");
	oTemp.open();
	m_sFitTempFile = oTemp.fileName();
	oTemp.close();
	
	QStringList oArgs;
	oArgs << sImageFile << m_sFitTempFile;
	m_oFitProcess->start(m_sFaceFitPath, oArgs, QProcess::ReadOnly);
	showStatusMessage(tr("Face fit started. Please wait..."), 0);
}

// +-----------------------------------------------------------
void ft::MainWindow::onFitError(QProcess::ProcessError eError)
{
	Q_UNUSED(eError);
	QFile::remove(m_sFitTempFile);
	m_sFitTempFile = "";
	showStatusMessage(tr("The face fit utility executable failed to execute. Please check its configuration."));
}

// +-----------------------------------------------------------
void ft::MainWindow::onFitFinished(int iExitCode, QProcess::ExitStatus eExitStatus)
{
	QFileInfo oFileInfo(m_sFitTempFile);
	if (!oFileInfo.exists())
	{
		showStatusMessage(tr("The face fit utility could not fit the landmarks to this image."));
		QFile::remove(m_sFitTempFile);
		m_sFitTempFile = "";
		return;
	}

	vector<QPointF> vPoints = Utils::readFaceFitPointsFile(m_sFitTempFile);
	if (vPoints.size() == 0)
	{
		showStatusMessage(tr("The face fit utility could not fit the landmarks to this image."));
		QFile::remove(m_sFitTempFile);
		m_sFitTempFile = "";
		return;
	}

	QFile::remove(m_sFitTempFile);
	m_sFitTempFile = "";


	// Get the selected face annotation dataset
	ChildWindow *pChild = (ChildWindow*)ui->tabWidget->currentWidget();
	if (!pChild) // Sanity check
		return;

	// Reposition the features according to the face-fit results
	pChild->positionFeatures(vPoints);
	showStatusMessage(tr("Face fit completed successfully."));
}

// +-----------------------------------------------------------
void ft::MainWindow::on_actionExportPointsFile_triggered()
{
	ChildWindow *pChild = (ChildWindow*)ui->tabWidget->currentWidget();
	if (!pChild) // Sanity check
		return;

	QList<FaceFeatureNode *> lFeats = pChild->getFaceFeatures();
	if(lFeats.size() == 0)
	{
		QMessageBox::critical(this, tr("Error exporting data"), tr("The exporting can not be done because there are no landmarks to export."), QMessageBox::Ok);
		return;
	}

	QString sFileName = QFileDialog::getSaveFileName(this, tr("Export CSIRO points file..."), windowFilePath(), tr("CSIRO Face Analysis SDK points file (*.pts);; All files (*.*)"));
	if (sFileName.length())
	{
		QFile oFile(sFileName);
		if (!oFile.open(QFile::WriteOnly | QFile::Text))
		{
			QMessageBox::critical(this, tr("Error exporting data"), tr("The exporting failed because it was not possible to write to file [%1].").arg(sFileName), QMessageBox::Ok);
			return;
		}

		QTextStream oStream(&oFile);
		QString sData = QString("n_points: %1").arg(lFeats.size());
		oStream << sData << endl;
		oStream << "{" << endl;

		foreach(FaceFeatureNode *pFeat, lFeats)
		{
			sData = QString("%1\t%2").arg(pFeat->x()).arg(pFeat->y());
			oStream << sData << endl;
		}
		oStream << "}" << endl;
		oFile.close();
	}
}

// +-----------------------------------------------------------

#ifdef DLIB_INTEGRATION
void ft::MainWindow::on_actionDlibFitLandmarks_triggered()
{
	// Check for face detection model
	if (!m_oDlib.has_facedet_model() && !m_sDlibFaceDetModelFilename.isEmpty())
		dlibLoadFaceDetModel(m_sDlibFaceDetModelFilename);
	if (!m_oDlib.has_facedet_model())
		emit on_actionDlibSelectFaceDetModel_triggered();
	if (!m_oDlib.has_facedet_model())
		return;

	// Check for landmark localization model
	if (!m_oDlib.has_landmark_model() && !m_sDlibLandmarkLocModelFilename.isEmpty())
		dlibLoadLandmarkLocModel(m_sDlibLandmarkLocModelFilename);
	if (!m_oDlib.has_landmark_model())
		emit on_actionDlibSelectLandmarkModel_triggered();
	if (!m_oDlib.has_landmark_model())
		return;

	// Get the selected face annotation dataset
	ChildWindow *pChild = (ChildWindow*)ui->tabWidget->currentWidget();
	if (!pChild) // Sanity check
		return;

	// Get the path of the selected image in the dataset
	QModelIndex oSelectedImage = pChild->selectionModel()->currentIndex();
	QModelIndex oIdx = pChild->dataModel()->index(oSelectedImage.row(), 1);
	QString sImageFile = pChild->dataModel()->data(oIdx, Qt::DisplayRole).toString();

	// Run the algorithm
	showStatusMessage(tr("Running dlib face detection and landmark localization..."));
	std::vector<QPointF> vPoints;
	if (!m_oDlib.get_landmarks(sImageFile, vPoints))
		QMessageBox::critical(this, tr("Error"), tr("Cannot detect face!"));

	// Reposition the features according to the face-fit results
	pChild->positionFeatures(vPoints);

	// Connect landmarks if wanted
	if (ui->actionDlibConnectFeatures->isChecked())
	{
		typedef std::pair<int, int> P;
		std::vector<P> idx;
		if (vPoints.size() == 68)
		{
			// chin-line
			for (int i = 0; i < 16; ++i) idx.push_back(P(i, i + 1));
			// eye-brows
			for (int i = 17; i < 21; ++i) idx.push_back(P(i, i + 1));
			for (int i = 22; i < 26; ++i) idx.push_back(P(i, i + 1));
			// nose
			for (int i = 27; i < 30; ++i) idx.push_back(P(i, i + 1));
			for (int i = 31; i < 35; ++i) idx.push_back(P(i, i + 1));
			// eyes
			for (int i = 36; i < 41; ++i) idx.push_back(P(i, i + 1)); idx.push_back(P(36, 41));
			for (int i = 42; i < 47; ++i) idx.push_back(P(i, i + 1)); idx.push_back(P(42, 47));
			// mouth
			for (int i = 48; i < 59; ++i) idx.push_back(P(i, i + 1)); idx.push_back(P(48, 59));
			for (int i = 60; i < 67; ++i) idx.push_back(P(i, i + 1)); idx.push_back(P(60, 67));
		}
		pChild->connectFeatures(idx);
	}

	showStatusMessage(tr("Face fit completed successfully."));
}

void ft::MainWindow::on_actionDlibSelectFaceDetModel_triggered()
{
	QString sFileName = QFileDialog::getOpenFileName(this, tr("Select DLIB face detector model..."), windowFilePath(), tr("Serialized DLIB model (*.dat);; All files (*.*)"));
	if (sFileName.length())
		dlibLoadFaceDetModel(sFileName);
}

void ft::MainWindow::on_actionDlibSelectLandmarkModel_triggered()
{
	QString sFileName = QFileDialog::getOpenFileName(this, tr("Select DLIB shape predictor model..."), windowFilePath(), tr("Serialized DLIB model (*.dat);; All files (*.*)"));
	if (sFileName.length())
		dlibLoadLandmarkLocModel(sFileName);
}

void ft::MainWindow::dlibLoadFaceDetModel(const QString & sFileName)
{
	if (!sFileName.length())
		return;
	showStatusMessage(tr("Loading dlib face detection model..."));
	if (m_oDlib.set_facedet_model_filename(sFileName))
	{
		showStatusMessage(tr("Loading dlib face detection model... successful!"));
		m_sDlibFaceDetModelFilename = sFileName;
	}
	else
	{
		showStatusMessage("");
		QMessageBox::critical(this, tr("Error"), tr("Error loading model file %1!").arg(sFileName));
	}
}

void ft::MainWindow::dlibLoadLandmarkLocModel(const QString & sFileName)
{
	showStatusMessage(tr("Loading dlib landmark localization model..."));
	if (m_oDlib.set_landmark_model_filename(sFileName))
	{
		showStatusMessage(tr("Loading dlib landmark localization model... successful."));
		m_sDlibLandmarkLocModelFilename = sFileName;
	}
	else
	{
		showStatusMessage("");
		QMessageBox::critical(this, tr("Error"), tr("Error loading model file %1!").arg(sFileName));
	}
}
#endif

// +-----------------------------------------------------------
void ft::MainWindow::on_actionProject_triggered()
{
	QDesktopServices::openUrl(QUrl("https://github.com/luigivieira/Facial-Landmarks-Annotation-Tool.git"));
}

// +-----------------------------------------------------------
void ft::MainWindow::on_actionAbout_triggered()
{
	(new AboutWindow(this))->show();
}

// +-----------------------------------------------------------
void ft::MainWindow::on_tabCloseRequested(int iTabIndex)
{
	ChildWindow* pChild = (ChildWindow*) ui->tabWidget->widget(iTabIndex);

	if(pChild->isWindowModified())
	{
		QString sMsg = tr("There are pending changes in the face annotation dataset named [%1]. Do you wish to save it before closing?").arg(QFileInfo(pChild->windowFilePath()).baseName());
		QMessageBox::StandardButton oResp = QMessageBox::question(this, tr("Pending changes"), sMsg, QMessageBox::Yes|QMessageBox::No|QMessageBox::Cancel);

		// Do not close the tab if the user has chosen "cancel" or if she has chosen "yes" but then
		// cancelled the file save dialog
		ui->tabWidget->setCurrentIndex(iTabIndex);
		if(oResp == QMessageBox::Cancel || (oResp == QMessageBox::Yes && !saveCurrentFile()))
			return;
	}

	destroyChildWindow(pChild);

	if(ui->tabWidget->count() == 0) // no more tabs
	{
		ui->textFileName->setText("");
		ui->zoomSlider->blockSignals(true);
		ui->zoomSlider->setValue((ui->zoomSlider->maximum() - ui->zoomSlider->minimum()) / 2 + 1);
		ui->zoomSlider->blockSignals(false);
	}
}

// +-----------------------------------------------------------
void ft::MainWindow::on_tabChanged(int iTabIndex)
{
	Q_UNUSED(iTabIndex);
	updateUI();
}

// +-----------------------------------------------------------
void ft::MainWindow::on_actionAddImage_triggered()
{
	ChildWindow *pChild = (ChildWindow*) ui->tabWidget->currentWidget();
	if(!pChild)
		return;

    QStringList lsFiles = QFileDialog::getOpenFileNames(this, tr("Select face images..."), m_sLastPathUsed, tr("Common image files (*.bmp *.png *.jpg *.gif);; All files (*.*)"));
	if(lsFiles.size())
	{
		m_sLastPathUsed = QFileInfo(lsFiles[0]).absolutePath();
		pChild->dataModel()->addImages(lsFiles);
		if(!pChild->selectionModel()->currentIndex().isValid())
			pChild->selectionModel()->setCurrentIndex(pChild->dataModel()->index(0, 0), QItemSelectionModel::Select);
	}
}

// +-----------------------------------------------------------
void ft::MainWindow::on_actionRemoveImage_triggered()
{
	ChildWindow *pChild = (ChildWindow*) ui->tabWidget->currentWidget();
	if(!pChild)
		return;

	QModelIndexList lsSelected = pChild->selectionModel()->selectedRows();
	if(lsSelected.size() > 0)
	{
		QString sMsg;
		if(lsSelected.size() == 1)
			sMsg = tr("Do you confirm the removal of the selected image?");
		else
			sMsg = tr("Do you confirm the removal of %1 selected images?").arg(lsSelected.size());
		if(QMessageBox::question(this, tr("Confirmation"), sMsg, QMessageBox::Yes|QMessageBox::No) == QMessageBox::Yes)
		{
			QList<int> lIndexes;
			for(int i = 0; i < lsSelected.size(); i++)
				lIndexes.append(lsSelected[i].row());
			pChild->dataModel()->removeImages(lIndexes);
		}
	}
}

// +-----------------------------------------------------------
void ft::MainWindow::on_actionShowImagesList_triggered(bool bChecked)
{
	ui->dockImages->setVisible(bChecked);
}

// +-----------------------------------------------------------
void ft::MainWindow::on_actionShowImageProperties_triggered(bool bChecked)
{
	ui->dockProperties->setVisible(bChecked);
}

// +-----------------------------------------------------------
void ft::MainWindow::on_actionShowFeatures_triggered(bool bChecked)
{
	if(!bChecked)
	{
		ui->actionShowConnections->setChecked(false);
		ui->actionShowFeatureIDs->setChecked(false);
	}
	updateUI();
}

// +-----------------------------------------------------------
void ft::MainWindow::on_actionShowConnections_triggered(bool bChecked)
{
	if(bChecked && !ui->actionShowFeatures->isChecked())
		ui->actionShowFeatures->setChecked(true);
	updateUI();
}

// +-----------------------------------------------------------
void ft::MainWindow::on_actionShowFeatureIDs_triggered(bool bChecked)
{
	if(bChecked && !ui->actionShowFeatures->isChecked())
		ui->actionShowFeatures->setChecked(true);
	updateUI();
}

// +-----------------------------------------------------------
void ft::MainWindow::on_actionAddFeature_triggered()
{
	ChildWindow *pChild = (ChildWindow*) ui->tabWidget->currentWidget();
	if(!pChild)
		return;

	// The position of the mouse  is stored in this action data
	// if the action is called from a context menu in the face features
	// editor (see method FaceWidget::contextMenuEvent)
	QVariant vPos = ui->actionAddFeature->data();
	QPoint oPos;
	if(vPos.isValid())
		oPos = vPos.value<QPoint>();
	else
		oPos = QCursor::pos();

	pChild->addFeature(oPos);
}

// +-----------------------------------------------------------
void ft::MainWindow::on_actionRemoveFeature_triggered()
{
	ChildWindow *pChild = (ChildWindow*) ui->tabWidget->currentWidget();
	if(!pChild)
		return;

	QString sMsg = tr("Do you confirm the removal of the selected facial landmarks? This action can not be undone.");
	QMessageBox::StandardButton oResp = QMessageBox::question(this, tr("Confirmation"), sMsg, QMessageBox::Yes|QMessageBox::No);

	if(oResp == QMessageBox::Yes)
		pChild->removeSelectedFeatures();
}

// +-----------------------------------------------------------
void ft::MainWindow::on_actionConnectFeatures_triggered()
{
	ChildWindow *pChild = (ChildWindow*) ui->tabWidget->currentWidget();
	if(!pChild)
		return;

	pChild->connectFeatures();
}

// +-----------------------------------------------------------
void ft::MainWindow::on_actionDisconnectFeatures_triggered()
{
	ChildWindow *pChild = (ChildWindow*) ui->tabWidget->currentWidget();
	if(!pChild)
		return;

	pChild->disconnectFeatures();
}

// +-----------------------------------------------------------
void ft::MainWindow::showStatusMessage(const QString &sMsg, const int iTimeout)
{
	ui->statusBar->showMessage(sMsg, iTimeout);
}

// +-----------------------------------------------------------
int ft::MainWindow::getFilePageIndex(const QString &sFile)
{
	int iRet = -1;
	for(int iPage = 0; iPage < ui->tabWidget->count(); iPage++)
	{
		ChildWindow* pChild = (ChildWindow*) ui->tabWidget->widget(iPage);
		if(pChild->windowFilePath() == sFile)
		{
			iRet = iPage;
			break;
		}
	}
	return iRet;
}

// +-----------------------------------------------------------
void ft::MainWindow::setImageListView(QString sType)
{
	if(sType == "details")
	{
		m_pViewButton->setIcon(QIcon(":/icons/viewdetails"));
		ui->listImages->setVisible(false);
		ui->treeImages->setVisible(true);
	}
	else if(sType == "icons")
	{
		m_pViewButton->setIcon(QIcon(":/icons/viewicons"));
		ui->treeImages->setVisible(false);
		ui->listImages->setVisible(true);
	}
}

// +-----------------------------------------------------------
void ft::MainWindow::toggleImageListView()
{
	if(ui->treeImages->isVisible())
		setImageListView("icons");
	else
		setImageListView("details");
}

// +-----------------------------------------------------------
void ft::MainWindow::onChildUIUpdated(const QString sImageName, const int iZoomLevel)
{
	// Image file name
	ui->textFileName->setText(sImageName);
	ui->textFileName->moveCursor(QTextCursor::End);
	ui->textFileName->ensureCursorVisible();
			
	// Zoom level
	ui->zoomSlider->blockSignals(true);
	ui->zoomSlider->setValue(iZoomLevel);
	ui->zoomSlider->blockSignals(false);

	updateUI();
}

// +-----------------------------------------------------------
void ft::MainWindow::updateUI()
{
	// Setup the control variables
	ChildWindow *pChild = (ChildWindow*) ui->tabWidget->currentWidget();

	bool bFileOpened = pChild != NULL;
	bool bFileChanged = bFileOpened ? pChild->isWindowModified() : false;
	bool bItemsSelected = bFileOpened && (pChild->selectionModel()->currentIndex().isValid() || pChild->selectionModel()->selectedIndexes().size() > 0);
	bool bFileNotNew = bFileOpened && !pChild->property("new").toBool();

	QList<FaceFeatureNode*> lFeats;
	QList<FaceFeatureEdge*> lConns;
	if(bFileOpened)
	{
		lFeats = pChild->getSelectedFeatures();
		lConns = pChild->getSelectedConnections();
	}

	bool bFeaturesSelected = lFeats.size() > 0;
	bool bConnectionsSelected = lConns.size() > 0;
	bool bFeaturesConnectable = lFeats.size() == 2 && lConns.size() == 0;

	// Update the data and selection models
	if(bFileOpened)
	{
		ui->listImages->setModel(pChild->dataModel());
		ui->listImages->setSelectionModel(pChild->selectionModel());
		ui->treeImages->setModel(pChild->dataModel());
		ui->treeImages->setSelectionModel(pChild->selectionModel());
	}
	else
	{
		ui->listImages->setModel(NULL);
		ui->treeImages->setModel(NULL);
	}

	// Update the UI availability
	ui->actionSave->setEnabled(bFileChanged);
	ui->actionSaveAs->setEnabled(bFileNotNew);
	ui->actionImportImageDirPts->setEnabled(bFileOpened);
	ui->actionExportPts->setEnabled(bFileOpened);
	ui->actionAddImage->setEnabled(bFileOpened);
	ui->actionRemoveImage->setEnabled(bItemsSelected);
	ui->actionAddFeature->setEnabled(bFileOpened);
	ui->actionRemoveFeature->setEnabled(bFeaturesSelected);
	ui->actionConnectFeatures->setEnabled(bFeaturesConnectable);
	ui->actionDisconnectFeatures->setEnabled(bConnectionsSelected);
	ui->actionFitLandmarks->setEnabled(bItemsSelected);
	ui->actionExportPointsFile->setEnabled(bItemsSelected);
	ui->actionDlibFitLandmarks->setEnabled(bItemsSelected);
	m_pViewButton->setEnabled(bFileOpened);
	ui->zoomSlider->setEnabled(bFileOpened);

	// Update the tab title and tooltip
	if(bFileOpened)
	{
		QString sTitle = QFileInfo(pChild->windowFilePath()).baseName() + (pChild->isWindowModified() ? "*" : "");
		ui->tabWidget->setTabText(ui->tabWidget->currentIndex(), sTitle);
		if(bFileNotNew) // Complete file path only if the file has been saved before
			ui->tabWidget->setTabToolTip(ui->tabWidget->currentIndex(), pChild->windowFilePath());
	}

	// Display of face features
	if(bFileOpened)
	{
		pChild->setDisplayFaceFeatures(ui->actionShowFeatures->isChecked());
		pChild->setDisplayConnections(ui->actionShowFeatures->isChecked() && ui->actionShowConnections->isChecked());
		pChild->setDisplayFeatureIDs(ui->actionShowFeatures->isChecked() && ui->actionShowFeatureIDs->isChecked());
	}

}

// +-----------------------------------------------------------
void ft::MainWindow::onSliderValueChanged(int iValue)
{
	ChildWindow *pChild = (ChildWindow*) ui->tabWidget->currentWidget();
	if(pChild)
		pChild->setZoomLevel(iValue);
}

// +-----------------------------------------------------------
void ft::MainWindow::onZoomLevelChanged(int iValue)
{
	ui->zoomSlider->blockSignals(true);
	ui->zoomSlider->setValue(iValue);
	ui->zoomSlider->blockSignals(false);
}

// +-----------------------------------------------------------
void ft::MainWindow::onUpdateUI()
{
	updateUI();
}

// +-----------------------------------------------------------
void ft::MainWindow::keyPressEvent(QKeyEvent *pEvent)
{
	ChildWindow *pChild = (ChildWindow*) ui->tabWidget->currentWidget();
	if(!pChild)
	{
		QMainWindow::keyPressEvent(pEvent);
		return;
	}

    switch(pEvent->key())
	{
		case Qt::Key_Plus:
			pChild->zoomIn();
			pEvent->accept();
			break;

		case Qt::Key_Minus:
			pChild->zoomOut();
			pEvent->accept();
			break;

		default:
			QMainWindow::keyPressEvent(pEvent);
    }
}

// +-----------------------------------------------------------
ft::ChildWindow* ft::MainWindow::createChildWindow(QString sFileName, bool bModified)
{
	ChildWindow *pChild = new ChildWindow(this);

	// Define the window attributes
	if(!sFileName.length())
	{
		sFileName = QString(tr("%1new face annotation dataset.fad")).arg(m_sLastPathUsed);
		bModified = true;
	}

	pChild->setWindowIcon(QIcon(":/icons/face-dataset"));
	pChild->setWindowFilePath(sFileName);
	pChild->setWindowModified(bModified);

	// Connect to its signals
	connect(pChild, SIGNAL(onUIUpdated(const QString, const int)), this, SLOT(onChildUIUpdated(const QString, const int)));
	connect(pChild, SIGNAL(onDataModified()), this, SLOT(onUpdateUI()));
	connect(pChild, SIGNAL(onFeaturesSelectionChanged()), this, SLOT(onUpdateUI()));

	// Create the context menu for the features editor, using the same actions from the main window
	QMenu *pContextMenu = new QMenu(pChild);
	pContextMenu->addAction(ui->actionAddFeature);
	pContextMenu->addAction(ui->actionRemoveFeature);
	pContextMenu->addAction(ui->actionConnectFeatures);
	pContextMenu->addAction(ui->actionDisconnectFeatures);
	pChild->setContextMenu(pContextMenu);

	// Add the window to the tab widget
	int iIndex = ui->tabWidget->addTab(pChild, pChild->windowIcon(), "");
	ui->tabWidget->setCurrentIndex(iIndex);

	return pChild;
}

// +-----------------------------------------------------------
void ft::MainWindow::destroyChildWindow(ChildWindow *pChild)
{
	int iTabIndex = ui->tabWidget->indexOf(pChild);
	ui->tabWidget->removeTab(iTabIndex);

	disconnect(pChild, SIGNAL(onUIUpdated(const QString, const int)), this, SLOT(onChildUIUpdated(const QString, const int)));
	disconnect(pChild, SIGNAL(onDataModified()), this, SLOT(onUpdateUI()));
	disconnect(pChild, SIGNAL(onFeaturesSelectionChanged()), this, SLOT(onUpdateUI()));

	delete pChild;
}