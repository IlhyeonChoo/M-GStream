#include "GaussianView.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "RenderUtils.hpp"
#include <core/graphics/GUI.hpp>
#include <thread>
#include <boost/asio.hpp>
#include <rasterizer.h>
#include <imgui_internal.h>
#include "RenderingSystem.hpp"
#include "projects/extended_gaussian/renderer/scene/GaussianInstance.hpp"
#include "RenderUtils.hpp"
#include "projects/extended_gaussian/renderer/cuda/TransformKernels.cuh"

namespace sibr {
	class BufferCopyRenderer
	{

	public:

		BufferCopyRenderer()
		{
			_shader.init("CopyShader",
				sibr::loadFile(sibr::getShadersDirectory("extended_gaussian") + "/copy.vert"),
				sibr::loadFile(sibr::getShadersDirectory("extended_gaussian") + "/copy.frag"));

			_flip.init(_shader, "flip");
			_width.init(_shader, "width");
			_height.init(_shader, "height");
		}

		void process(uint bufferID, IRenderTarget& dst, int width, int height, bool disableTest = true)
		{
			if (disableTest)
				glDisable(GL_DEPTH_TEST);
			else
				glEnable(GL_DEPTH_TEST);

			_shader.begin();
			_flip.send();
			_width.send();
			_height.send();

			dst.clear();
			dst.bind();

			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, bufferID);

			sibr::RenderUtility::renderScreenQuad();

			dst.unbind();
			_shader.end();
		}

		/** \return option to flip the texture when copying. */
		bool& flip() { return _flip.get(); }
		int& width() { return _width.get(); }
		int& height() { return _height.get(); }

	private:

		GLShader			_shader;
		GLuniform<bool>		_flip = false; ///< Flip the texture when copying.
		GLuniform<int>		_width = 1000;
		GLuniform<int>		_height = 800;
	};

	std::function<char* (size_t N)> resizeFunctional(void** ptr, size_t& S) {
		auto lambda = [ptr, &S](size_t N) {
			if (N > S)
			{
				if (*ptr)
					CUDA_SAFE_CALL(cudaFree(*ptr));
				CUDA_SAFE_CALL(cudaMalloc(ptr, 2 * N));
				S = 2 * N;
			}
			return reinterpret_cast<char*>(*ptr);
			};
		return lambda;
	}

	GaussianView::GaussianView(const RenderingSystem* p_owner, uint render_w, uint render_h, bool useInterop)
		: owner(p_owner), ViewBase(render_w, render_h) {

		copyRenderer = new BufferCopyRenderer();
		copyRenderer->flip() = true;
		copyRenderer->width() = render_w;
		copyRenderer->height() = render_h;

		CUDA_SAFE_CALL_ALWAYS(cudaMalloc((void**)&view_cuda, sizeof(sibr::Matrix4f)));
		CUDA_SAFE_CALL_ALWAYS(cudaMalloc((void**)&proj_cuda, sizeof(sibr::Matrix4f)));
		CUDA_SAFE_CALL_ALWAYS(cudaMalloc((void**)&cam_pos_cuda, 3 * sizeof(float)));
		CUDA_SAFE_CALL_ALWAYS(cudaMalloc((void**)&background_cuda, 3 * sizeof(float)));

		// Create GL buffer ready for CUDA/GL interop
		glCreateBuffers(1, &imageBuffer);
		glNamedBufferStorage(imageBuffer, render_w * render_h * 3 * sizeof(float), nullptr, GL_DYNAMIC_STORAGE_BIT);

		if (useInterop)
		{
			if (cudaPeekAtLastError() != cudaSuccess)
			{
				SIBR_ERR << "A CUDA error occurred in setup:" << cudaGetErrorString(cudaGetLastError()) << ". Please rerun in Debug to find the exact line!";
			}
			cudaGraphicsGLRegisterBuffer(&imageBufferCuda, imageBuffer, cudaGraphicsRegisterFlagsWriteDiscard);
			useInterop &= (cudaGetLastError() == cudaSuccess);
		}
		if (!useInterop)
		{
			fallback_bytes.resize(render_w * render_h * 3 * sizeof(float));
			cudaMalloc(&fallbackBufferCuda, fallback_bytes.size());
			_interop_failed = true;
		}

		background[0] = 0.f;
		background[1] = 0.f;
		background[2] = 0.f;

		geomBufferFunc = resizeFunctional(&geomPtr, allocdGeom);
		binningBufferFunc = resizeFunctional(&binningPtr, allocdBinning);
		imgBufferFunc = resizeFunctional(&imgPtr, allocdImg);
	}

	void GaussianView::onRenderIBR(sibr::IRenderTarget& dst, const sibr::Camera& eye)
	{
		glClearNamedBufferData(imageBuffer, GL_RGB32F, GL_RGB, GL_FLOAT, background);

		size_t totalCount = 0;
		lastSkippedInstances_ = 0;
		auto& gaussian_instances = owner->getScene()->getInstances();
		for (auto& it : gaussian_instances) {
			const auto* gpuField = it.second->getGPUField();
			if (gpuField) {
				totalCount += gpuField->count;
			}
		}

		if (totalCount == 0)
		{
			return;
		}

		resizeWorldBuffersIfNeeded(totalCount);

		// Convert view and projection to target coordinate system
		auto view_mat = eye.view();
		auto proj_mat = eye.viewproj();
		view_mat.row(1) *= -1;
		view_mat.row(2) *= -1;
		proj_mat.row(1) *= -1;

		// Compute additional view parameters
		float tan_fovy = tan(eye.fovy() * 0.5f);
		float tan_fovx = tan_fovy * eye.aspect();

		// Copy frame-dependent data to GPU		
		CUDA_SAFE_CALL_ALWAYS(cudaMemcpy(background_cuda, background, 3 * sizeof(float), cudaMemcpyHostToDevice));
		CUDA_SAFE_CALL(cudaMemcpy(view_cuda, view_mat.data(), sizeof(sibr::Matrix4f), cudaMemcpyHostToDevice));
		CUDA_SAFE_CALL(cudaMemcpy(proj_cuda, proj_mat.data(), sizeof(sibr::Matrix4f), cudaMemcpyHostToDevice));
		CUDA_SAFE_CALL(cudaMemcpy(cam_pos_cuda, &eye.position(), sizeof(float) * 3, cudaMemcpyHostToDevice));

		// Copy gaussians to world buffer
		for (auto& it : gaussian_instances) {
			auto inst = it.second.get();
			auto field = inst->getGPUField();
			if (!field)
			{
				if (!inst->getAssetId().empty()) {
					++lastSkippedInstances_;
				}
				continue;
			}

			AppendGaussianToWorld(
				field,
				current_world_gausians_count,
				inst->getOrigin()->getPosition(),
				inst->getOrigin()->getEular(),
				inst->getOrigin()->getScale()
			);
		}

		float* image_cuda = nullptr;
		if (!_interop_failed)
		{
			// Map OpenGL buffer resource for use with CUDA
			size_t bytes;
			CUDA_SAFE_CALL(cudaGraphicsMapResources(1, &imageBufferCuda));
			CUDA_SAFE_CALL(cudaGraphicsResourceGetMappedPointer((void**)&image_cuda, &bytes, imageBufferCuda));
		}
		else
		{
			image_cuda = fallbackBufferCuda;
		}

		CudaRasterizer::Rasterizer::forward(
			geomBufferFunc,
			binningBufferFunc,
			imgBufferFunc,
			totalCount,
			3,
			16,
			background_cuda,
			_resolution.x(), _resolution.y(),
			world_pos_cuda,
			world_shs_cuda,
			nullptr,
			world_opacity_cuda,
			world_scale_cuda,
			1.0f,
			world_rot_cuda,
			nullptr,
			view_cuda,
			proj_cuda,
			cam_pos_cuda,
			tan_fovx,
			tan_fovy,
			false,
			image_cuda,
			antialiasing,
			nullptr,
			world_rect_cuda,
			nullptr,
			nullptr
		);

		if (!_interop_failed) {
			CUDA_SAFE_CALL(cudaGraphicsUnmapResources(1, &imageBufferCuda));
		}
		else {
			CUDA_SAFE_CALL(cudaMemcpy(fallback_bytes.data(), fallbackBufferCuda, fallback_bytes.size(), cudaMemcpyDeviceToHost));
			glNamedBufferSubData(imageBuffer, 0, fallback_bytes.size(), fallback_bytes.data());
		}

		copyRenderer->process(imageBuffer, dst, _resolution.x(), _resolution.y(), false);

		// Reset gaussian count
		current_world_gausians_count = 0;
	}

	void GaussianView::onGUI() {}

	size_t GaussianView::lastSkippedInstances() const
	{
		return lastSkippedInstances_;
	}

	GaussianView::~GaussianView()
	{
		// Cleanup
		if (!_interop_failed)
		{
			cudaGraphicsUnregisterResource(imageBufferCuda);
		}
		else
		{
			cudaFree(fallbackBufferCuda);
		}
		glDeleteBuffers(1, &imageBuffer);

		if (geomPtr)
			cudaFree(geomPtr);
		if (binningPtr)
			cudaFree(binningPtr);
		if (imgPtr)
			cudaFree(imgPtr);


		cudaFree(view_cuda);
		cudaFree(proj_cuda);
		cudaFree(cam_pos_cuda);
		cudaFree(background_cuda);

		cudaFree(world_pos_cuda);
		cudaFree(world_rot_cuda);
		cudaFree(world_scale_cuda);
		cudaFree(world_opacity_cuda);
		cudaFree(world_shs_cuda);
		cudaFree(world_rect_cuda);

		delete copyRenderer;
	}

	bool GaussianView::resizeWorldBuffersIfNeeded(size_t count)
	{
		if (count > max_gaussians_count) {
			if (world_pos_cuda)     CUDA_SAFE_CALL(cudaFree(world_pos_cuda));
			if (world_rot_cuda)     CUDA_SAFE_CALL(cudaFree(world_rot_cuda));
			if (world_scale_cuda)   CUDA_SAFE_CALL(cudaFree(world_scale_cuda));
			if (world_opacity_cuda) CUDA_SAFE_CALL(cudaFree(world_opacity_cuda));
			if (world_shs_cuda)     CUDA_SAFE_CALL(cudaFree(world_shs_cuda));
			if (world_rect_cuda)    CUDA_SAFE_CALL(cudaFree(world_rect_cuda));

			max_gaussians_count = static_cast<size_t>(count * 1.2f);

			const int sh_degree = 3;
			const int sh_coeffs = (sh_degree + 1) * (sh_degree + 1);

			CUDA_SAFE_CALL(cudaMalloc((void**)&world_pos_cuda, max_gaussians_count * 3 * sizeof(float)));
			CUDA_SAFE_CALL(cudaMalloc((void**)&world_rot_cuda, max_gaussians_count * 4 * sizeof(float)));
			CUDA_SAFE_CALL(cudaMalloc((void**)&world_scale_cuda, max_gaussians_count * 3 * sizeof(float)));
			CUDA_SAFE_CALL(cudaMalloc((void**)&world_opacity_cuda, max_gaussians_count * 1 * sizeof(float)));
			CUDA_SAFE_CALL(cudaMalloc((void**)&world_shs_cuda, max_gaussians_count * sh_coeffs * 3 * sizeof(float)));

			CUDA_SAFE_CALL(cudaMalloc((void**)&world_rect_cuda, max_gaussians_count * 2 * sizeof(int)));

			std::cout << "[VRAM] Resized ALL World Buffers for " << max_gaussians_count << " gaussians." << std::endl;

			return true;
		}
		return false;
	}

	void GaussianView::AppendGaussianToWorld(
		const GPUGaussianField* source,
		size_t offset,
		const Vector3f& position,
		const Vector3f& eular,
		float scale
	) {
		if (!source)
		{
			return;
		}

		current_world_gausians_count += source->count;

		TransformPosRotScaleSHsToWorld(
			source,
			offset,
			position,
			eular,
			scale,
			world_pos_cuda,
			world_rot_cuda,
			world_scale_cuda,
			world_shs_cuda
		);

		appendSHsToWorld(
			source->shs_cuda,
			source->count,
			offset,
			3
		);

		appendOpacitiesToWorld(
			source->opacity_cuda,
			source->count,
			offset
		);
	}

	void GaussianView::TransformPosRotScaleSHsToWorld(
		const GPUGaussianField* source,
		size_t offset,
		const Vector3f& position,
		const Vector3f& eular,
		float scale,
		float* w_pos_ptr, float* w_rot_ptr, float* w_scale_ptr, float* w_shs_ptr
	) {
		const int count = source->count;
		const float degToRad = 3.1415926535f / 180.0f;

		Eigen::AngleAxisf roll(eular.x() * degToRad, Vector3f::UnitX());
		Eigen::AngleAxisf pitch(eular.y() * degToRad, Vector3f::UnitY());
		Eigen::AngleAxisf yaw(eular.z() * degToRad, Vector3f::UnitZ());
		Quaternionf q = roll * pitch * yaw;

		Matrix4f worldMat = Matrix4f::Identity();
		worldMat.block<3, 3>(0, 0) = q.toRotationMatrix() * scale;
		worldMat.block<3, 1>(0, 3) = position;

		Matrix4x4 gpuMat;
		Matrix4f rowMajor = worldMat.transpose();
		memcpy(gpuMat.m, rowMajor.data(), sizeof(float) * 16);

		float4 instRot = { q.w(), q.x(), q.y(), q.z() };

		int threads = 256;
		int blocks = (count + threads - 1) / threads;

		launchTransformKernel(
			blocks, threads, count, (int)offset,
			(const float3*)source->pos_cuda,
			(const float4*)source->rot_cuda,
			(const float3*)source->scale_cuda,
			(float3*)w_pos_ptr, (float4*)w_rot_ptr, (float3*)w_scale_ptr,
			gpuMat, instRot, scale
		);
	}

	void GaussianView::appendSHsToWorld(const float* src_shs, int count, int offset, int sh_degree)
	{
		int sh_coeffs = (sh_degree + 1) * (sh_degree + 1);
		int floats_per_gaussian = sh_coeffs * 3;

		float* dst_ptr = world_shs_cuda + (offset * floats_per_gaussian);
		size_t size_bytes = count * floats_per_gaussian * sizeof(float);

		CUDA_SAFE_CALL(cudaMemcpyAsync(dst_ptr, src_shs, size_bytes, cudaMemcpyDeviceToDevice));
	}

	void GaussianView::appendOpacitiesToWorld(const float* src_opacities, int count, int offset)
	{
		float* dst_ptr = world_opacity_cuda + offset;
		size_t size_bytes = count * sizeof(float);

		CUDA_SAFE_CALL(cudaMemcpyAsync(dst_ptr, src_opacities, size_bytes, cudaMemcpyDeviceToDevice));
	}

}

