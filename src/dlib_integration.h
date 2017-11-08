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

#ifdef DLIB_DNN_FACE_DETECTOR
#include <dlib/dnn.h>
#else
#include <dlib/image_processing/frontal_face_detector.h>
#endif
#include <dlib/image_processing/shape_predictor.h>

#include <QString>
#include <QPoint>


class DlibFeatureLocalization
{
public:
	DlibFeatureLocalization();

	bool set_facedet_model_filename(const QString & sModelFn);
	bool has_facedet_model() const;

	bool set_landmark_model_filename(const QString & sModelFn);
	bool has_landmark_model() const;

	bool get_landmarks(const QString & sImageFn, std::vector<QPointF> &vPoints);

protected:

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

#endif

#endif // DLIB_INTEGRATION_H
