/*
 * transform.h
 *
 *  Created on: Feb 17, 2026
 *      Author: mad
 */

#ifndef INCLUDE_MMPILOT_TRANSFORM_H_
#define INCLUDE_MMPILOT_TRANSFORM_H_

#include <mmpilot/math.h>
#include <mmpilot/value.h>


namespace mmpilot {

class Transform2D : public Value {
public:
	float scale = 1;
	Mat2f rot = Mat2f::Identity();	// rotation
	Vec2f pos = Vec2f::Zero();		// translation

	void add(const Transform2D& delta)
	{
		pos   += scale * (rot * delta.pos);
		rot    = rot * delta.rot;
		scale *= delta.scale;
		normalize_rot(rot);
	}

	Transform2D& transform(float alpha)
	{
		return transform(get_rotation_matrix(alpha));
	}

	Transform2D& transform(const Mat2f& R)
	{
		rot = R * rot;
		pos = R * pos;
		return *this;
	}

	friend Transform2D operator*(const Mat2f& R, const Transform2D& T)
	{
		return Transform2D(T).transform(R);
	}

	void set_rot(float alpha)
	{
		rot = get_rotation_matrix(alpha);
	}

	Vec2f apply(const Vec2f& p) const
	{
		return pos + rot * (p * scale);
	}

	Vec2f apply(float x, float y) const
	{
		return apply(Vec2f(x, y));
	}

	Transform2D inverse() const
	{
		Transform2D inv;
		inv.scale = 1 / scale;
		inv.rot   = rot.transpose();
		inv.pos   = -(inv.scale * (inv.rot * pos));
		normalize_rot(inv.rot);
		return inv;
	}

};


} // mmpilot

#endif /* INCLUDE_MMPILOT_TRANSFORM_H_ */
