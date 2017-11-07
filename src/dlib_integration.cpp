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

#ifdef DLIB_INTEGRATION
#include "dlib_integration.h"

#include <dlib/image_io.h>


DlibFeatureLocalization::DlibFeatureLocalization()
{
	face_detector = dlib::get_frontal_face_detector();
}

bool DlibFeatureLocalization::set_landmark_model_filename(const QString & sModelFn)
{
	try
	{
		dlib::deserialize(sModelFn.toStdString()) >> lm_localizer;
	}
	catch (...)
	{
		return false;
	}
	
	return true;
}

bool DlibFeatureLocalization::has_landmark_model() const
{
	return (lm_localizer.num_parts() > 0);
}

bool DlibFeatureLocalization::get_landmarks(const QString & sImageFn, std::vector<QPointF>& vPoints)
{
	// TODO: convert data (image already loaded by QT)

	if (lm_localizer.num_parts() == 0)
		return false;

	// load image
	dlib::array2d<dlib::rgb_pixel> img;
	dlib::load_image(img, sImageFn.toStdString());

	// detect faces
	std::vector<dlib::rectangle> dets = face_detector(img);
	if (dets.empty())
		return false;
	
	// select biggest face
	dlib::rectangle face_det;
	size_t size = 0;
	for each (const dlib::rectangle & rect in dets)
	{
		if (rect.width() > size)
		{
			face_det = rect;
			size = rect.width();
		}
	}

	// apply shape predictor
	dlib::full_object_detection shape = lm_localizer(img, face_det);
	vPoints.clear(); vPoints.reserve(shape.num_parts());
	for (size_t i = 0; i < shape.num_parts(); ++i)
	{
		dlib::point & p = shape.part(i);	// drops accuracy (interface only provides integer coordinates)
		vPoints.push_back(QPointF(p.x(), p.y()));
	}

	return true;
}

#endif
