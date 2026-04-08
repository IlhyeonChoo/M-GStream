#include "CameraPoseAdapter.hpp"

#include <cmath>

namespace {

	constexpr float kPi = 3.14159265358979323846f;
	constexpr float kMinVectorNorm = 1e-6f;
	constexpr float kParallelThreshold = 0.999f;

	bool isFiniteFloat(float value)
	{
		return std::isfinite(value) != 0;
	}

	bool isFiniteVector(const sibr::Vector3f& value)
	{
		return isFiniteFloat(value.x()) && isFiniteFloat(value.y()) && isFiniteFloat(value.z());
	}

} // namespace

namespace sibr {

	bool ValidateRemoteCameraPose(const RemoteCameraPose& pose, std::string& error)
	{
		error.clear();

		if (!isFiniteVector(pose.position)) {
			error = "Camera position must contain only finite values.";
			return false;
		}
		if (!isFiniteVector(pose.forward)) {
			error = "Camera forward vector must contain only finite values.";
			return false;
		}
		if (!isFiniteVector(pose.up)) {
			error = "Camera up vector must contain only finite values.";
			return false;
		}
		if (!isFiniteFloat(pose.fovy)) {
			error = "Camera fovy must be finite.";
			return false;
		}
		if (pose.fovy <= 0.0f || pose.fovy >= kPi) {
			error = "Camera fovy must be in the open interval (0, pi).";
			return false;
		}

		const float forwardNorm = pose.forward.norm();
		if (forwardNorm <= kMinVectorNorm) {
			error = "Camera forward vector must be non-zero.";
			return false;
		}

		const float upNorm = pose.up.norm();
		if (upNorm <= kMinVectorNorm) {
			error = "Camera up vector must be non-zero.";
			return false;
		}

		const Vector3f forward = pose.forward / forwardNorm;
		const Vector3f up = pose.up / upNorm;
		const float alignment = std::fabs(forward.dot(up));
		if (alignment >= kParallelThreshold) {
			error = "Camera forward and up vectors must not be parallel or near-parallel.";
			return false;
		}

		return true;
	}

	bool TryBuildInputCamera(const RemoteCameraPose& pose, sibr::InputCamera& camera, std::string& error)
	{
		if (!ValidateRemoteCameraPose(pose, error)) {
			return false;
		}

		const Vector3f forward = pose.forward.normalized();
		const Vector3f up = pose.up.normalized();
		const Vector3f right = forward.cross(up).normalized();
		const Vector3f correctedUp = right.cross(forward).normalized();

		if (!isFiniteVector(right) || !isFiniteVector(correctedUp) ||
			right.norm() <= kMinVectorNorm || correctedUp.norm() <= kMinVectorNorm) {
			error = "Camera pose did not produce a valid orthonormal basis.";
			return false;
		}

		float aspect = camera.aspect();
		if (!isFiniteFloat(aspect) || aspect <= 0.0f) {
			aspect = 1.0f;
		}

		float znear = camera.znear();
		if (!isFiniteFloat(znear) || znear <= 0.0f) {
			znear = 0.1f;
		}

		float zfar = camera.zfar();
		if (!isFiniteFloat(zfar) || zfar <= znear) {
			zfar = 1000.0f;
		}

		camera.setLookAt(pose.position, pose.position + forward, correctedUp);
		camera.fovy(pose.fovy);
		camera.aspect(aspect);
		camera.znear(znear);
		camera.zfar(zfar);

		error.clear();
		return true;
	}

	RemoteCameraPose ExportRemoteCameraPose(const sibr::InputCamera& camera)
	{
		RemoteCameraPose pose;
		pose.position = camera.position();
		pose.forward = camera.dir();
		pose.up = camera.up();
		pose.fovy = camera.fovy();
		return pose;
	}

} // namespace sibr
