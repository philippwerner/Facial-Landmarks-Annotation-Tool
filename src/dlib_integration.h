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

#ifdef DLIB_INTEGRATION
#include <dlib/image_processing/frontal_face_detector.h>
#include <dlib/image_processing/shape_predictor.h>

#include <QString>
#include <QPoint>


class DlibFeatureLocalization
{
public:
	DlibFeatureLocalization();

	bool set_landmark_model_filename(const QString & sModelFn);
	bool has_landmark_model() const;

	bool get_landmarks(const QString & sImageFn, std::vector<QPointF> &vPoints);

protected:
	dlib::frontal_face_detector face_detector;
	dlib::shape_predictor lm_localizer;
};

#endif

#endif // DLIB_INTEGRATION_H
