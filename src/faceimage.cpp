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

#include "faceimage.h"

#include <QApplication>
#include <QFileInfo>
#include <QTextStream>
#include <QMessageBox>

using namespace std;

// +-----------------------------------------------------------
ft::FaceImage::FaceImage(const QString &sFileName)
{
	m_sFileName = sFileName;
}

// +-----------------------------------------------------------
ft::FaceImage::~FaceImage()
{
	clear();
}

// +-----------------------------------------------------------
void ft::FaceImage::copyFeaturesFrom(const FaceImage * oImg)
{
	m_vFeatures.clear();
	for each (FaceFeature * src_ff in oImg->m_vFeatures)
	{
		m_vFeatures.push_back(new FaceFeature(*src_ff));
	}
}

// +-----------------------------------------------------------
void ft::FaceImage::clear()
{
	foreach(FaceFeature *pFeature, m_vFeatures)
		delete pFeature;
	m_vFeatures.clear();
}

// +-----------------------------------------------------------
QString ft::FaceImage::fileName() const
{
	return m_sFileName;
}

// +-----------------------------------------------------------
void ft::FaceImage::setFileName(QString sFileName)
{
	m_sFileName = sFileName;
}

// +-----------------------------------------------------------
bool ft::FaceImage::loadFromXML(const QDomElement &oElement, QString &sMsgError, int iNumExpectedFeatures)
{
	// Check the element name
	if(oElement.tagName() != "Sample")
	{
		sMsgError = QString(QApplication::translate("FaceImage", "invalid node name [%1] - expected node '%2'").arg(oElement.tagName(), "Sample"));
		return false;
	}

	// Read the file name
	QString sFile = oElement.attribute("fileName");
	if(sFile == "")
	{
		sMsgError = QString(QApplication::translate("FaceImage", "the attribute '%1' does not exist or it contains an invalid value").arg("fileName"));
		return false;
	}

	// Read the face features
	// Sample images
	QDomElement oFeatures = oElement.firstChildElement("Features");
	if(oFeatures.isNull() || oFeatures.childNodes().count() != iNumExpectedFeatures)
	{
		sMsgError = QString(QApplication::translate("FaceImage", "the node '%1' does not exist or it contains less children nodes than expected").arg("Features"));
		return false;
	}

	vector<FaceFeature*> vFeatures;
	for(QDomElement oElement = oFeatures.firstChildElement(); !oElement.isNull(); oElement = oElement.nextSiblingElement())
	{
		FaceFeature *pFeature = new FaceFeature();
		if(!pFeature->loadFromXML(oElement, sMsgError))
		{
			foreach(FaceFeature *pFeat, vFeatures)
				delete(pFeat);
			delete pFeature;

			return false;
		}
		vFeatures.push_back(pFeature);
	}

	clear();
	m_sFileName = sFile;
	m_vFeatures = vFeatures;
	return true;
}

// +-----------------------------------------------------------
void ft::FaceImage::saveToXML(QDomElement &oParent) const
{
	// Add the "Sample" node
	QDomElement oSample = oParent.ownerDocument().createElement("Sample");
	oParent.appendChild(oSample);

	// Define it's attributes
	oSample.setAttribute("fileName", m_sFileName);

	// Add the "Features" subnode
	QDomElement oFeatures = oParent.ownerDocument().createElement("Features");
	oSample.appendChild(oFeatures);

	// Add the nodes for the features
	foreach(FaceFeature *pFeat, m_vFeatures)
		pFeat->saveToXML(oFeatures);
}

// +-----------------------------------------------------------
QPixmap ft::FaceImage::pixMap() const
{
	QPixmap oRet;
	oRet.load(m_sFileName);
	return oRet;
}

// +-----------------------------------------------------------
bool ft::FaceImage::loadPtsFile(QString & errstr, int iNumExpectedFeatures)
{
	int last_dot = m_sFileName.lastIndexOf(".");
	QString fn = m_sFileName.mid(0, last_dot) + ".pts";
	int n_points = -1;
	QString keyword;
	int value;
	;

	QFile file(fn);
	if (!file.open(QIODevice::ReadOnly))
	{
		errstr = QString(QApplication::translate("FaceImage", "Cannot open file '%1'!").arg(fn));
		return false;
	}

	QTextStream in(&file);

	while (!in.atEnd())
	{
		in >> keyword;
		keyword = keyword.toLower();
		if (keyword == "version:")
		{
			in >> value;
			if (value != 1)
			{
				errstr = QString(QApplication::translate("FaceImage", "File '%1' has unsupported version!").arg(fn));
				return false;
			}
		}
		else if (keyword == "n_points:")
		{
			in >> value;
			if (value < 1 || value > 10000)
			{
				errstr = QString(QApplication::translate("FaceImage", "File '%1' has unsupported n_points '%2'!").arg(fn).arg(value));
				return false;
			}
			n_points = value;
		}
		else if (keyword == "{")
		{
			break;
		}
		else 
		{
			errstr = QString(QApplication::translate("FaceImage", "File '%1' parse error!").arg(fn));
			return false;
		}
	}

	if (n_points < 1)
	{
		errstr = QString(QApplication::translate("FaceImage", "File '%1' lacks n_points info!").arg(fn));
		return false;
	}

	if (iNumExpectedFeatures > 0 && iNumExpectedFeatures != n_points)
	{
		errstr = QString(QApplication::translate("FaceImage", "File '%1' n_points=%2 is unexpected! Expected n_points=%3!").arg(fn).arg(n_points).arg(iNumExpectedFeatures));
		return false;
	}

	// read points
	float x, y;
	m_vFeatures.clear();
	for (int i = 0; i < n_points; ++i)
	{
		in >> x >> y;
		m_vFeatures.push_back(new FaceFeature(i, x, y));
	}

	if (in.atEnd())
	{
		errstr = QString(QApplication::translate("FaceImage", "File '%1' parse error! File ended too early.").arg(fn));
		return false;
	}

	return true;
}

// +-----------------------------------------------------------
bool ft::FaceImage::savePtsFile(bool keep_backup)
{
	int last_dot = m_sFileName.lastIndexOf(".");
	QString fn = m_sFileName.mid(0, last_dot) + ".pts";

	// todo:
	// - add change bool per FaceImage, set in ft::ChildWindow::updateFeaturesInDataset()
	// - check for changes, keep backup %1_backup001.pts
	// - GUI: option whether to save backup

	if (m_vFeatures.empty())
		return true;

	QFile oFile(fn);
	if (!oFile.open(QFile::WriteOnly | QFile::Text))
		return false;

	QTextStream oStream(&oFile);
	oStream << "version: 1" << endl;
	oStream << QString("n_points: %1").arg(m_vFeatures.size()) << endl;
	oStream << "{" << endl;

	foreach(FaceFeature *pFeat, m_vFeatures)
	{
		oStream << QString("%1\t%2").arg(pFeat->x()).arg(pFeat->y()) << endl;
	}
	oStream << "}" << endl;
	oFile.close();

	return true;
}

// +-----------------------------------------------------------
ft::FaceFeature* ft::FaceImage::addFeature(int iID, float x, float y)
{
	FaceFeature *pFeat = new FaceFeature(iID, x, y);
	m_vFeatures.push_back(pFeat);
	return pFeat;
}

// +-----------------------------------------------------------
ft::FaceFeature* ft::FaceImage::getFeature(const int iIndex) const
{
	if(iIndex < 0 || iIndex >= (int) m_vFeatures.size())
		return NULL;
	return m_vFeatures[iIndex];
}

// +-----------------------------------------------------------
const std::vector<ft::FaceFeature*> & ft::FaceImage::getFeatures() const
{
	return m_vFeatures;
}

// +-----------------------------------------------------------
bool ft::FaceImage::removeFeature(const int iIndex)
{
	if(iIndex < 0 || iIndex >= (int) m_vFeatures.size())
		return false;

	FaceFeature *pFeat = m_vFeatures[iIndex];
	m_vFeatures.erase(m_vFeatures.begin() + iIndex);
	delete pFeat;

	return true;
}

// +-----------------------------------------------------------
bool ft::FaceImage::connectFeatures(int iIDSource, int iIDTarget)
{
	if (iIDSource < 0 || iIDSource >= (int)m_vFeatures.size())
		return false;
	if (iIDTarget < 0 || iIDTarget >= (int)m_vFeatures.size())
		return false;

	FaceFeature *pSource = m_vFeatures[iIDSource];
	FaceFeature *pTarget = m_vFeatures[iIDTarget];
	pSource->connectTo(pTarget);
	return true;
}

// +-----------------------------------------------------------
bool ft::FaceImage::disconnectFeatures(int iIDSource, int iIDTarget)
{
	if (iIDSource < 0 || iIDSource >= (int)m_vFeatures.size())
		return false;
	if (iIDTarget < 0 || iIDTarget >= (int)m_vFeatures.size())
		return false;

	FaceFeature *pSource = m_vFeatures[iIDSource];
	FaceFeature *pTarget = m_vFeatures[iIDTarget];
	pSource->disconnectFrom(pTarget);
	return true;
}