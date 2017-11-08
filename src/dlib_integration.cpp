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

#ifdef DLIB_DNN_FACE_DETECTOR
#include <dlib/dnn.h>
#else
#include <dlib/image_processing/frontal_face_detector.h>
#endif

#include <dlib/image_processing/shape_predictor.h>
#include <dlib/image_io.h>


// Proxy datatype to speed up compilation by excluding dlib headers from other files
struct Data
{
#ifdef DLIB_DNN_FACE_DETECTOR
	template <long num_filters, typename SUBNET> using con5d = dlib::con<num_filters, 5, 5, 2, 2, SUBNET>;
	template <long num_filters, typename SUBNET> using con5 = dlib::con<num_filters, 5, 5, 1, 1, SUBNET>;

	template <typename SUBNET> using downsampler = dlib::relu<dlib::affine<con5d<32, dlib::relu<dlib::affine<con5d<32, dlib::relu<dlib::affine<con5d<16, SUBNET>>>>>>>>>;
	template <typename SUBNET> using rcon5 = dlib::relu<dlib::affine<con5<45, SUBNET>>>;

	using net_type = dlib::loss_mmod<dlib::con<1, 9, 9, 1, 1, rcon5<rcon5<rcon5<downsampler<dlib::input_rgb_image_pyramid<dlib::pyramid_down<6>>>>>>>>;

	net_type face_detector;
	bool face_detector_loaded = false;
#else
	dlib::frontal_face_detector face_detector;
#endif

	dlib::shape_predictor lm_localizer;
};


DlibFeatureLocalization::DlibFeatureLocalization()
{
	m_pData = new Data;
#ifndef DLIB_DNN_FACE_DETECTOR
	face_detector = dlib::get_frontal_face_detector();
#endif
}

DlibFeatureLocalization::~DlibFeatureLocalization()
{
	Data * d = static_cast<Data *>(m_pData);
	if (d)
		delete d;
}

bool DlibFeatureLocalization::set_facedet_model_filename(const QString & sModelFn)
{
	Data * d = static_cast<Data *>(m_pData);
#ifdef DLIB_DNN_FACE_DETECTOR
	try
	{
		dlib::deserialize(sModelFn.toStdString()) >> d->face_detector;
	}
	catch (...)
	{
		return false;
	}
	d->face_detector_loaded = true;
#endif

	return true;
}

bool DlibFeatureLocalization::has_facedet_model() const
{
#ifdef DLIB_DNN_FACE_DETECTOR
	Data * d = static_cast<Data *>(m_pData);
	return d->face_detector_loaded;
#else
	return true;
#endif
}

bool DlibFeatureLocalization::set_landmark_model_filename(const QString & sModelFn)
{
	Data * d = static_cast<Data *>(m_pData);
	try
	{
		dlib::deserialize(sModelFn.toStdString()) >> d->lm_localizer;
	}
	catch (...)
	{
		return false;
	}
	
	return true;
}

bool DlibFeatureLocalization::has_landmark_model() const
{
	Data * d = static_cast<Data *>(m_pData);
	return (d->lm_localizer.num_parts() > 0);
}

bool DlibFeatureLocalization::get_landmarks(const QString & sImageFn, std::vector<QPointF>& vPoints)
{
	Data * d = static_cast<Data *>(m_pData);
	// TODO: convert data (image already loaded by QT) for speedup

	if (!has_landmark_model() || !has_facedet_model())
		return false;

	// load image
	dlib::matrix<dlib::rgb_pixel> img;
	dlib::load_image(img, sImageFn.toStdString());

	// detect faces
#ifdef DLIB_DNN_FACE_DETECTOR	
	std::vector<dlib::matrix<dlib::rgb_pixel>> images(1);
	std::vector<std::vector<dlib::mmod_rect>> dets_list;
	std::vector<dlib::mmod_rect> dets;
	images[0] = img;
	dets_list = d->face_detector(images);
	dets = dets_list[0];
#else
	std::vector<dlib::rectangle> dets = face_detector(img);
#endif
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
	dlib::full_object_detection shape = d->lm_localizer(img, face_det);
	vPoints.clear(); vPoints.reserve(shape.num_parts());
	for (size_t i = 0; i < shape.num_parts(); ++i)
	{
		dlib::point & p = shape.part(i);	// drops accuracy (interface only provides integer coordinates)
		vPoints.push_back(QPointF(p.x(), p.y()));
	}

	return true;
}

#endif
