/*
* Copyright (C) 2017 Philipp Werner (http://philipp-werner.info)
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

#ifndef DLIB_INTEGRATION_H
#define DLIB_INTEGRATION_H

#define DLIB_DNN_FACE_DETECTOR

#ifdef DLIB_INTEGRATION


#include <QString>
#include <QPoint>
#include <vector>


class DlibFeatureLocalization
{
public:
	DlibFeatureLocalization();
	~DlibFeatureLocalization();

	bool set_facedet_model_filename(const QString & sModelFn);
	bool has_facedet_model() const;

	bool set_landmark_model_filename(const QString & sModelFn);
	bool has_landmark_model() const;

	bool get_landmarks(const QString & sImageFn, std::vector<QPointF> &vPoints);

protected:
	// Speed up compilation by excluding dlib headers from most files
	void * m_pData;
};

#endif

#endif // DLIB_INTEGRATION_H
