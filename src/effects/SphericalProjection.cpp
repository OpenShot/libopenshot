/**
 * @file
 * @brief Source file for SphericalProjection effect class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2025 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "SphericalProjection.h"
#include "Exceptions.h"

#include <algorithm>
#include <cmath>
#include <omp.h>
#include <vector>

using namespace openshot;

SphericalProjection::SphericalProjection()
	: yaw(0.0), pitch(0.0), roll(0.0), fov(90.0), in_fov(180.0),
		projection_mode(0), invert(0), input_model(INPUT_EQUIRECT), interpolation(3)
{
	init_effect_details();
}

SphericalProjection::SphericalProjection(Keyframe new_yaw, Keyframe new_pitch,
										 Keyframe new_roll, Keyframe new_fov)
	: yaw(new_yaw), pitch(new_pitch), roll(new_roll), fov(new_fov),
		in_fov(180.0), projection_mode(0), invert(0),
		input_model(INPUT_EQUIRECT), interpolation(3)
{
	init_effect_details();
}

void SphericalProjection::init_effect_details()
{
	InitEffectInfo();
	info.class_name = "SphericalProjection";
	info.name = "Spherical Projection";
	info.description =
		"Flatten and reproject 360° or fisheye inputs into a rectilinear view with yaw, pitch, roll, and FOV. Supports Equirect and multiple fisheye lens models.";
	info.has_audio = false;
	info.has_video = true;
}

namespace {
	inline double cubic_interp(double p0, double p1, double p2, double p3,
								 double t)
	{
		double a0 = -0.5 * p0 + 1.5 * p1 - 1.5 * p2 + 0.5 * p3;
		double a1 = p0 - 2.5 * p1 + 2.0 * p2 - 0.5 * p3;
		double a2 = -0.5 * p0 + 0.5 * p2;
		double a3 = p1;
		return ((a0 * t + a1) * t + a2) * t + a3;
	}
} // namespace

std::shared_ptr<openshot::Frame>
SphericalProjection::GetFrame(std::shared_ptr<openshot::Frame> frame,
								int64_t frame_number) {
	auto img = frame->GetImage();
	if (img->format() != QImage::Format_ARGB32)
	*img = img->convertToFormat(QImage::Format_ARGB32);

	int W = img->width(), H = img->height();
	int bpl = img->bytesPerLine();
	uchar *src = img->bits();

	QImage output(W, H, QImage::Format_ARGB32);
	output.fill(Qt::black);
	uchar *dst = output.bits();
	int dst_bpl = output.bytesPerLine();

	// Keyframes / angles
	const double DEG = M_PI / 180.0;
	double yaw_r        = -yaw.GetValue(frame_number)     * DEG; // drag right -> look right
	double pitch_r    =    pitch.GetValue(frame_number) * DEG; // drag up        -> look up
	double roll_r     = -roll.GetValue(frame_number)    * DEG; // positive slider -> clockwise on screen
	double in_fov_r =    in_fov.GetValue(frame_number) * DEG;
	double out_fov_r=    fov.GetValue(frame_number)        * DEG;

	// Apply invert as a 180° yaw for equirect inputs (camera-centric; no mirroring)
	if (input_model == INPUT_EQUIRECT && invert == INVERT_BACK) {
		yaw_r += M_PI;
	}

	// Rotation R = Ry(yaw) * Rx(pitch). (Roll applied in screen space.)
	double sy = sin(yaw_r),    cy = cos(yaw_r);
	double sp = sin(pitch_r),cp = cos(pitch_r);

	double r00 =    cy;
	double r01 =    sy * sp;
	double r02 =    sy * cp;

	double r10 =    0.0;
	double r11 =    cp;
	double r12 = -sp;

	double r20 = -sy;
	double r21 =    cy * sp;
	double r22 =    cy * cp;

	// Keep roll clockwise on screen regardless of facing direction
	double roll_sign = (r22 >= 0.0) ? 1.0 : -1.0;

	// Perspective scalars (rectilinear)
	double hx = tan(out_fov_r * 0.5);
	double vy = hx * double(H) / W;

	auto q = [](double a) { return std::llround(a * 1e6); };
	bool recompute = uv_map.empty() || W != cached_width || H != cached_height ||
					 q(yaw_r) != q(cached_yaw) ||
					 q(pitch_r) != q(cached_pitch) ||
					 q(roll_r) != q(cached_roll) ||
					 q(in_fov_r) != q(cached_in_fov) ||
					 q(out_fov_r) != q(cached_out_fov) ||
					 input_model != cached_input_model ||
					 projection_mode != cached_projection_mode ||
					 invert != cached_invert;

	if (recompute) {
	uv_map.resize(W * H * 2);

#pragma omp parallel for schedule(static)
	for (int yy = 0; yy < H; yy++) {
		double ndc_y = (2.0 * (yy + 0.5) / H - 1.0) * vy;

		for (int xx = 0; xx < W; xx++) {
		double uf = -1.0, vf = -1.0;

		const bool out_is_rect =
			(projection_mode == MODE_RECT_SPHERE || projection_mode == MODE_RECT_HEMISPHERE);

		if (!out_is_rect) {
			// ---------------- FISHEYE OUTPUT ----------------
			double cx = (xx + 0.5) - W * 0.5;
			double cy_dn = (yy + 0.5) - H * 0.5;
			double R    = 0.5 * std::min(W, H);

			// screen plane, Y-up; apply roll by -roll (clockwise), adjusted by roll_sign
			double rx =    cx / R;
			double ry_up = -cy_dn / R;
			double cR = cos(roll_r), sR = sin(roll_r) * roll_sign;
			double rxr =    cR * rx + sR * ry_up;
			double ryr = -sR * rx + cR * ry_up;

			double r_norm = std::sqrt(rxr * rxr + ryr * ryr);
			if (r_norm <= 1.0) {
			double theta_max = out_fov_r * 0.5;
			double theta = 0.0;
			switch (projection_mode) {
				case MODE_FISHEYE_EQUIDISTANT:
				// r ∝ θ
				theta = r_norm * theta_max;
				break;
				case MODE_FISHEYE_EQUISOLID:
				// r ∝ 2 sin(θ/2)
				theta = 2.0 * std::asin(std::clamp(r_norm * std::sin(theta_max * 0.5), -1.0, 1.0));
				break;
				case MODE_FISHEYE_STEREOGRAPHIC:
				// r ∝ 2 tan(θ/2)
				theta = 2.0 * std::atan(r_norm * std::tan(theta_max * 0.5));
				break;
				case MODE_FISHEYE_ORTHOGRAPHIC:
				// r ∝ sin(θ)
				theta = std::asin(std::clamp(r_norm * std::sin(theta_max), -1.0, 1.0));
				break;
				default:
				theta = r_norm * theta_max;
				break;
			}

			// NOTE: Y was upside-down; fix by using +ryr (not -ryr)
			double phi = std::atan2(ryr, rxr);

			// Camera ray from fisheye output
			double vx = std::sin(theta) * std::cos(phi);
			double vy2= std::sin(theta) * std::sin(phi);
			double vz = -std::cos(theta);

			// Rotate into world
			double dx = r00 * vx + r01 * vy2 + r02 * vz;
			double dy = r10 * vx + r11 * vy2 + r12 * vz;
			double dz = r20 * vx + r21 * vy2 + r22 * vz;

			project_input(dx, dy, dz, in_fov_r, W, H, uf, vf);
			} else {
			uf = vf = -1.0; // outside disk
			}

		} else {
			// ---------------- RECTILINEAR OUTPUT ----------------
			double ndc_x = (2.0 * (xx + 0.5) / W - 1.0) * hx;

			// screen plane Y-up; roll by -roll (clockwise), adjusted by roll_sign
			double sx = ndc_x;
			double sy_up = -ndc_y;
			double cR = cos(roll_r), sR = sin(roll_r) * roll_sign;
			double rx =    cR * sx + sR * sy_up;
			double ry = -sR * sx + cR * sy_up;

			// Camera ray (camera looks down -Z)
			double vx = rx, vy2 = ry, vz = -1.0;
			double inv_len = 1.0 / std::sqrt(vx*vx + vy2*vy2 + vz*vz);
			vx *= inv_len; vy2 *= inv_len; vz *= inv_len;

			// Rotate into world
			double dx = r00 * vx + r01 * vy2 + r02 * vz;
			double dy = r10 * vx + r11 * vy2 + r12 * vz;
			double dz = r20 * vx + r21 * vy2 + r22 * vz;

			project_input(dx, dy, dz, in_fov_r, W, H, uf, vf);
		}

		int idx = 2 * (yy * W + xx);
		uv_map[idx]         = (float)uf;
		uv_map[idx + 1] = (float)vf;
		}
	}

	cached_width    = W;
	cached_height = H;
	cached_yaw        = yaw_r;
	cached_pitch    = pitch_r;
	cached_roll     = roll_r;
	cached_in_fov = in_fov_r;
	cached_out_fov= out_fov_r;
	cached_input_model         = input_model;
	cached_projection_mode = projection_mode;
	cached_invert                    = invert;
	}

	// Auto sampler selection (uses enums)
	int sampler = interpolation;
	if (interpolation == INTERP_AUTO) {
	double coverage_r =
		(projection_mode == MODE_RECT_SPHERE)         ? 2.0 * M_PI :
		(projection_mode == MODE_RECT_HEMISPHERE) ? M_PI :
													in_fov_r; // rough heuristic otherwise
	double ppd_src = W / coverage_r;
	double ppd_out = W / out_fov_r;
	double ratio     = ppd_out / ppd_src;
	if            (ratio < 0.8)    sampler = INTERP_AUTO;     // mipmaps path below
	else if (ratio <= 1.2) sampler = INTERP_BILINEAR;
	else                                     sampler = INTERP_BICUBIC;
	}

	// Build mipmaps only if needed (box)
	std::vector<QImage> mipmaps;
	if (sampler == INTERP_AUTO) {
	mipmaps.push_back(*img);
	for (int level = 1; level < 4; ++level) {
		const QImage &prev = mipmaps[level - 1];
		if (prev.width() <= 1 || prev.height() <= 1) break;
		int w = prev.width() / 2, h = prev.height() / 2;
		QImage next(w, h, QImage::Format_ARGB32);
		uchar *nb = next.bits(); int nbpl = next.bytesPerLine();
		const uchar *pb = prev.bits(); int pbpl = prev.bytesPerLine();
		for (int y = 0; y < h; y++) {
		for (int x = 0; x < w; x++) {
			for (int c = 0; c < 4; c++) {
			int p00 = pb[(2*y)     * pbpl + (2*x)     * 4 + c];
			int p10 = pb[(2*y)     * pbpl + (2*x+1) * 4 + c];
			int p01 = pb[(2*y+1) * pbpl + (2*x)     * 4 + c];
			int p11 = pb[(2*y+1) * pbpl + (2*x+1) * 4 + c];
			nb[y * nbpl + x * 4 + c] = (p00 + p10 + p01 + p11) / 4;
			}
		}
		}
		mipmaps.push_back(next);
	}
	}

#pragma omp parallel for schedule(static)
	for (int yy = 0; yy < H; yy++) {
	uchar *dst_row = dst + yy * dst_bpl;
	for (int xx = 0; xx < W; xx++) {
		int idx = 2 * (yy * W + xx);
		double uf = uv_map[idx];
		double vf = uv_map[idx + 1];
		uchar *d = dst_row + xx * 4;

		if (input_model == INPUT_EQUIRECT && projection_mode == MODE_RECT_SPHERE) {
		uf = std::fmod(std::fmod(uf, W) + W, W);
		vf = std::clamp(vf, 0.0, (double)H - 1);
		} else if (input_model == INPUT_EQUIRECT && projection_mode == MODE_RECT_HEMISPHERE) {
		uf = std::clamp(uf, 0.0, (double)W - 1);
		vf = std::clamp(vf, 0.0, (double)H - 1);
		} else if (uf < 0 || uf >= W || vf < 0 || vf >= H) {
		d[0] = d[1] = d[2] = 0; d[3] = 0;
		continue;
		}

		if (sampler == INTERP_NEAREST) {
		int x0 = std::clamp(int(std::floor(uf)), 0, W - 1);
		int y0 = std::clamp(int(std::floor(vf)), 0, H - 1);
		uchar *s = src + y0 * bpl + x0 * 4;
		d[0]=s[0]; d[1]=s[1]; d[2]=s[2]; d[3]=s[3];
		} else if (sampler == INTERP_BILINEAR) {
		int x0 = std::clamp(int(std::floor(uf)), 0, W - 1);
		int y0 = std::clamp(int(std::floor(vf)), 0, H - 1);
		int x1 = std::clamp(x0 + 1, 0, W - 1);
		int y1 = std::clamp(y0 + 1, 0, H - 1);
		double dxr = uf - x0, dyr = vf - y0;
		uchar *p00 = src + y0 * bpl + x0 * 4;
		uchar *p10 = src + y0 * bpl + x1 * 4;
		uchar *p01 = src + y1 * bpl + x0 * 4;
		uchar *p11 = src + y1 * bpl + x1 * 4;
		for (int c = 0; c < 4; c++) {
			double v0 = p00[c] * (1 - dxr) + p10[c] * dxr;
			double v1 = p01[c] * (1 - dxr) + p11[c] * dxr;
			d[c] = uchar(v0 * (1 - dyr) + v1 * dyr + 0.5);
		}
		} else if (sampler == INTERP_BICUBIC) {
		int x1 = std::clamp(int(std::floor(uf)), 0, W - 1);
		int y1 = std::clamp(int(std::floor(vf)), 0, H - 1);
		double tx = uf - x1, ty = vf - y1;
		for (int c = 0; c < 4; c++) {
			double col[4];
			for (int j = -1; j <= 2; j++) {
			int y = std::clamp(y1 + j, 0, H - 1);
			double row[4];
			for (int i = -1; i <= 2; i++) {
				int x = std::clamp(x1 + i, 0, W - 1);
				row[i + 1] = src[y * bpl + x * 4 + c];
			}
			col[j + 1] = cubic_interp(row[0], row[1], row[2], row[3], tx);
			}
			double val = cubic_interp(col[0], col[1], col[2], col[3], ty);
			d[c] = uchar(std::clamp(val, 0.0, 255.0) + 0.5);
		}
		} else { // INTERP_AUTO -> mipmaps + bilinear
		double uf_dx = 0.0, vf_dx = 0.0, uf_dy = 0.0, vf_dy = 0.0;
		if (xx + 1 < W) { uf_dx = uv_map[idx + 2] - uf; vf_dx = uv_map[idx + 3] - vf; }
		if (yy + 1 < H) { uf_dy = uv_map[idx + 2 * W] - uf; vf_dy = uv_map[idx + 2 * W + 1] - vf; }
		double scale_x = std::sqrt(uf_dx*uf_dx + vf_dx*vf_dx);
		double scale_y = std::sqrt(uf_dy*uf_dy + vf_dy*vf_dy);
		double scale     = std::max(scale_x, scale_y);
		int level = 0;
		if (scale > 1.0)
			level = std::min<int>(std::floor(std::log2(scale)), (int)mipmaps.size() - 1);
		const QImage &lvl = mipmaps[level];
		int Wl = lvl.width(), Hl = lvl.height();
		int bpl_l = lvl.bytesPerLine();
		const uchar *srcl = lvl.bits();
		double uf_l = uf / (1 << level);
		double vf_l = vf / (1 << level);
		int x0 = std::clamp(int(std::floor(uf_l)), 0, Wl - 1);
		int y0 = std::clamp(int(std::floor(vf_l)), 0, Hl - 1);
		int x1 = std::clamp(x0 + 1, 0, Wl - 1);
		int y1 = std::clamp(y0 + 1, 0, Hl - 1);
		double dxr = uf_l - x0, dyr = vf_l - y0;
		const uchar *p00 = srcl + y0 * bpl_l + x0 * 4;
		const uchar *p10 = srcl + y0 * bpl_l + x1 * 4;
		const uchar *p01 = srcl + y1 * bpl_l + x0 * 4;
		const uchar *p11 = srcl + y1 * bpl_l + x1 * 4;
		for (int c = 0; c < 4; c++) {
			double v0 = p00[c] * (1 - dxr) + p10[c] * dxr;
			double v1 = p01[c] * (1 - dxr) + p11[c] * dxr;
			d[c] = uchar(v0 * (1 - dyr) + v1 * dyr + 0.5);
		}
		}
	}
	}

	*img = output;
	return frame;
}

void SphericalProjection::project_input(double dx, double dy, double dz,
										double in_fov_r, int W, int H,
										double &uf, double &vf) const {
	if (input_model == INPUT_EQUIRECT) {
		// Center (-Z) -> lon=0; +X (screen right) -> +lon
		double lon = std::atan2(dx, -dz);
		double lat = std::asin(std::clamp(dy, -1.0, 1.0));

		if (projection_mode == MODE_RECT_HEMISPHERE)
			lon = std::clamp(lon, -M_PI / 2.0, M_PI / 2.0);

		double horiz_span = (projection_mode == MODE_RECT_HEMISPHERE) ? M_PI : 2.0 * M_PI;
		double lon_offset = (projection_mode == MODE_RECT_HEMISPHERE) ? M_PI / 2.0 : M_PI;
		uf = ((lon + lon_offset) / horiz_span) * W;

		// Image Y grows downward: north (lat = +π/2) at top
		vf = (M_PI / 2.0 - lat) / M_PI * H;
		return;
	}

	// -------- Fisheye inputs --------
	// Optical axis default is -Z; "Invert" flips hemisphere.
	const double ax = 0.0, ay = 0.0;
	double az = -1.0;
	if (invert == INVERT_BACK) az = 1.0;

	double cos_t    = std::clamp(dx * ax + dy * ay + dz * az, -1.0, 1.0);
	double theta    = std::acos(cos_t);
	double tmax     = std::max(1e-6, in_fov_r * 0.5);

	double r_norm = 0.0;
	switch (input_model) {
	case INPUT_FEQ_EQUIDISTANT:            r_norm =                theta            / tmax; break;
	case INPUT_FEQ_EQUISOLID:                r_norm = std::sin(theta*0.5) / std::max(1e-12, std::sin(tmax*0.5)); break;
	case INPUT_FEQ_STEREOGRAPHIC:        r_norm = std::tan(theta*0.5) / std::max(1e-12, std::tan(tmax*0.5)); break;
	case INPUT_FEQ_ORTHOGRAPHIC:         r_norm = std::sin(theta)         / std::max(1e-12, std::sin(tmax));         break;
	default:                                                 r_norm =                theta            / tmax; break;
	}

	// Azimuth in camera XY; final Y is downward -> subtract sine in vf
	double phi = std::atan2(dy, dx);

	double R     = 0.5 * std::min(W, H);
	double rpx = r_norm * R;
	uf = W * 0.5 + rpx * std::cos(phi);
	vf = H * 0.5 - rpx * std::sin(phi);
}

std::string SphericalProjection::Json() const
{
	return JsonValue().toStyledString();
}

Json::Value SphericalProjection::JsonValue() const
{
	Json::Value root = EffectBase::JsonValue();
	root["type"] = info.class_name;
	root["yaw"] = yaw.JsonValue();
	root["pitch"] = pitch.JsonValue();
	root["roll"] = roll.JsonValue();
	root["fov"] = fov.JsonValue();
	root["in_fov"] = in_fov.JsonValue();
	root["projection_mode"] = projection_mode;
	root["invert"] = invert;
	root["input_model"] = input_model;
	root["interpolation"] = interpolation;
	return root;
}

void SphericalProjection::SetJson(const std::string value)
{
	try
	{
		Json::Value root = openshot::stringToJson(value);
		SetJsonValue(root);
	}
	catch (...)
	{
		throw InvalidJSON("Invalid JSON for SphericalProjection");
	}
}

void SphericalProjection::SetJsonValue(const Json::Value root)
{
	EffectBase::SetJsonValue(root);

	if (!root["yaw"].isNull()) yaw.SetJsonValue(root["yaw"]);
	if (!root["pitch"].isNull()) pitch.SetJsonValue(root["pitch"]);
	if (!root["roll"].isNull()) roll.SetJsonValue(root["roll"]);
	if (!root["fov"].isNull()) fov.SetJsonValue(root["fov"]);
	if (!root["in_fov"].isNull()) in_fov.SetJsonValue(root["in_fov"]);

	if (!root["projection_mode"].isNull())
		projection_mode = root["projection_mode"].asInt();

	if (!root["invert"].isNull())
		invert = root["invert"].asInt();

	if (!root["input_model"].isNull())
		input_model = root["input_model"].asInt();

	if (!root["interpolation"].isNull())
		interpolation = root["interpolation"].asInt();

	// Clamp to enum options
	projection_mode = std::clamp(projection_mode,
									(int)MODE_RECT_SPHERE,
									(int)MODE_FISHEYE_ORTHOGRAPHIC);
	invert                    = std::clamp(invert, (int)INVERT_NORMAL, (int)INVERT_BACK);
	input_model         = std::clamp(input_model, (int)INPUT_EQUIRECT, (int)INPUT_FEQ_ORTHOGRAPHIC);
	interpolation     = std::clamp(interpolation, (int)INTERP_NEAREST, (int)INTERP_AUTO);

	// any property change should invalidate cached UV map
	uv_map.clear();


	// any property change should invalidate cached UV map
	uv_map.clear();
}
std::string SphericalProjection::PropertiesJSON(int64_t requested_frame) const
{
	Json::Value root = BasePropertiesJSON(requested_frame);

	root["yaw"]     = add_property_json("Yaw",     yaw.GetValue(requested_frame),     "float", "degrees", &yaw,    -180, 180, false, requested_frame);
	root["pitch"] = add_property_json("Pitch", pitch.GetValue(requested_frame), "float", "degrees", &pitch,-180, 180, false, requested_frame);
	root["roll"]    = add_property_json("Roll",    roll.GetValue(requested_frame),    "float", "degrees", &roll, -180, 180, false, requested_frame);

	root["fov"]        = add_property_json("Out FOV", fov.GetValue(requested_frame),        "float", "degrees", &fov,         0, 179, false, requested_frame);
	root["in_fov"] = add_property_json("In FOV",    in_fov.GetValue(requested_frame), "float", "degrees", &in_fov,    1, 360, false, requested_frame);

	root["projection_mode"] = add_property_json("Projection Mode", projection_mode, "int", "", nullptr,
							(int)MODE_RECT_SPHERE, (int)MODE_FISHEYE_ORTHOGRAPHIC, false, requested_frame);
	root["projection_mode"]["choices"].append(add_property_choice_json("Sphere",                                     (int)MODE_RECT_SPHERE,                    projection_mode));
	root["projection_mode"]["choices"].append(add_property_choice_json("Hemisphere",                             (int)MODE_RECT_HEMISPHERE,            projection_mode));
	root["projection_mode"]["choices"].append(add_property_choice_json("Fisheye: Equidistant",         (int)MODE_FISHEYE_EQUIDISTANT,    projection_mode));
	root["projection_mode"]["choices"].append(add_property_choice_json("Fisheye: Equisolid",             (int)MODE_FISHEYE_EQUISOLID,        projection_mode));
	root["projection_mode"]["choices"].append(add_property_choice_json("Fisheye: Stereographic",     (int)MODE_FISHEYE_STEREOGRAPHIC,projection_mode));
	root["projection_mode"]["choices"].append(add_property_choice_json("Fisheye: Orthographic",        (int)MODE_FISHEYE_ORTHOGRAPHIC, projection_mode));

	root["invert"] = add_property_json("Invert View", invert, "int", "", nullptr, 0, 1, false, requested_frame);
	root["invert"]["choices"].append(add_property_choice_json("Normal", 0, invert));
	root["invert"]["choices"].append(add_property_choice_json("Invert", 1, invert));

	root["input_model"] = add_property_json("Input Model", input_model, "int", "", nullptr, INPUT_EQUIRECT, INPUT_FEQ_ORTHOGRAPHIC, false, requested_frame);
	root["input_model"]["choices"].append(add_property_choice_json("Equirectangular (Panorama)", INPUT_EQUIRECT,                    input_model));
	root["input_model"]["choices"].append(add_property_choice_json("Fisheye: Equidistant",                INPUT_FEQ_EQUIDISTANT,     input_model));
	root["input_model"]["choices"].append(add_property_choice_json("Fisheye: Equisolid",                    INPUT_FEQ_EQUISOLID,         input_model));
	root["input_model"]["choices"].append(add_property_choice_json("Fisheye: Stereographic",            INPUT_FEQ_STEREOGRAPHIC, input_model));
	root["input_model"]["choices"].append(add_property_choice_json("Fisheye: Orthographic",             INPUT_FEQ_ORTHOGRAPHIC,    input_model));

	root["interpolation"] = add_property_json("Interpolation", interpolation, "int", "", nullptr, 0, 3, false, requested_frame);
	root["interpolation"]["choices"].append(add_property_choice_json("Nearest",    0, interpolation));
	root["interpolation"]["choices"].append(add_property_choice_json("Bilinear", 1, interpolation));
	root["interpolation"]["choices"].append(add_property_choice_json("Bicubic",    2, interpolation));
	root["interpolation"]["choices"].append(add_property_choice_json("Auto",         3, interpolation));

	return root.toStyledString();
}
