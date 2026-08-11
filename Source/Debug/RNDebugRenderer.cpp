//
//  RNDebugRenderer.cpp
//  Rayne
//
//  Copyright 2026 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#include "RNDebugRenderer.h"
#include "../Math/RNConstants.h"
#include "../Rendering/RNMaterial.h"
#include "../Rendering/RNMesh.h"
#include "../Rendering/RNModel.h"
#include "../Rendering/RNRenderer.h"
#include "../Scene/RNEntity.h"
#include "../Scene/RNScene.h"

namespace RN
{
	RNDefineMeta(DebugTextBackend, Object)
	RNDefineMeta(DebugRenderer, SceneAttachment)

	static DebugRenderer *__defaultDebugRenderer = nullptr;

	enum class DebugCommandType : uint8
	{
		Point,
		Line,
		Ray,
		AABB,
		Sphere
	};

	struct DebugCommand
	{
		DebugCommandType type;
		PositionType origin;
		Vector3 from;
		Vector3 to;
		RN::AABB aabb;
		float radius;
		DebugDrawOptions options;
		float remaining;
		bool rendered;
	};

	struct DebugBatch
	{
		Entity *entity = nullptr;
		Mesh *mesh = nullptr;
		Material *material = nullptr;
		size_t vertexCount = 0;
		size_t indexCount = 0;
		bool depthTest = true;
	};

	struct DebugGeometry
	{
		PositionType origin;
		std::vector<Vector3> positions;
		std::vector<Color> colors;
		std::vector<uint32> indices;
	};

	struct DebugRenderer::Internals
	{
		std::vector<DebugCommand> commands;
		DebugBatch depthBatch;
		DebugBatch noDepthBatch;
		DebugTextBackend *textBackend = nullptr;
		bool installed = false;
	};

	class DebugGeometryBuilder
	{
	public:
		static float GetThickness(const DebugDrawOptions &options)
		{
			return std::max(options.thickness, 0.0001f);
		}

		static void AppendVertex(DebugGeometry &geometry, const Vector3 &position, const Color &color)
		{
			geometry.positions.push_back(position);
			geometry.colors.push_back(color);
		}

		static void AppendBox(DebugGeometry &geometry, const Vector3 &min, const Vector3 &max, const Color &color)
		{
			const uint32 base = static_cast<uint32>(geometry.positions.size());

			AppendVertex(geometry, Vector3(min.x, min.y, min.z), color);
			AppendVertex(geometry, Vector3(max.x, min.y, min.z), color);
			AppendVertex(geometry, Vector3(max.x, max.y, min.z), color);
			AppendVertex(geometry, Vector3(min.x, max.y, min.z), color);
			AppendVertex(geometry, Vector3(min.x, min.y, max.z), color);
			AppendVertex(geometry, Vector3(max.x, min.y, max.z), color);
			AppendVertex(geometry, Vector3(max.x, max.y, max.z), color);
			AppendVertex(geometry, Vector3(min.x, max.y, max.z), color);

			const uint32 localIndices[] = {
				0, 2, 1, 0, 3, 2,
				4, 5, 6, 4, 6, 7,
				0, 1, 5, 0, 5, 4,
				1, 2, 6, 1, 6, 5,
				2, 3, 7, 2, 7, 6,
				3, 0, 4, 3, 4, 7
			};

			for(uint32 index : localIndices)
				geometry.indices.push_back(base + index);
		}

		static void AppendPoint(DebugGeometry &geometry, const Vector3 &position, const DebugDrawOptions &options)
		{
			const float halfSize = GetThickness(options) * 0.5f;
			AppendBox(geometry, position - Vector3(halfSize), position + Vector3(halfSize), options.color);
		}

		static void AppendLine(DebugGeometry &geometry, const Vector3 &from, const Vector3 &to, const DebugDrawOptions &options)
		{
			Vector3 direction = to - from;
			const float length = direction.GetLength();
			if(length <= k::EpsilonFloat)
			{
				AppendPoint(geometry, from, options);
				return;
			}

			direction /= length;
			Vector3 reference = Math::FastAbs(direction.y) < 0.9f ? Vector3(0.0f, 1.0f, 0.0f) : Vector3(1.0f, 0.0f, 0.0f);
			Vector3 side0 = direction.GetCrossProduct(reference).GetNormalized(GetThickness(options) * 0.5f);
			Vector3 side1 = direction.GetCrossProduct(side0).GetNormalized(GetThickness(options) * 0.5f);

			const uint32 base = static_cast<uint32>(geometry.positions.size());

			AppendVertex(geometry, from - side0 - side1, options.color);
			AppendVertex(geometry, from + side0 - side1, options.color);
			AppendVertex(geometry, from + side0 + side1, options.color);
			AppendVertex(geometry, from - side0 + side1, options.color);
			AppendVertex(geometry, to - side0 - side1, options.color);
			AppendVertex(geometry, to + side0 - side1, options.color);
			AppendVertex(geometry, to + side0 + side1, options.color);
			AppendVertex(geometry, to - side0 + side1, options.color);

			const uint32 localIndices[] = {
				0, 1, 5, 0, 5, 4,
				1, 2, 6, 1, 6, 5,
				2, 3, 7, 2, 7, 6,
				3, 0, 4, 3, 4, 7,
				0, 2, 1, 0, 3, 2,
				4, 5, 6, 4, 6, 7
			};

			for(uint32 index : localIndices)
				geometry.indices.push_back(base + index);
		}

		static void AppendRay(DebugGeometry &geometry, const Vector3 &from, const Vector3 &to, const DebugDrawOptions &options)
		{
			Vector3 direction = to - from;
			const float length = direction.GetLength();
			if(length <= k::EpsilonFloat)
			{
				AppendPoint(geometry, from, options);
				return;
			}

			direction /= length;
			const float headLength = std::min(length * 0.25f, GetThickness(options) * 6.0f);
			Vector3 reference = Math::FastAbs(direction.y) < 0.9f ? Vector3(0.0f, 1.0f, 0.0f) : Vector3(1.0f, 0.0f, 0.0f);
			Vector3 side0 = direction.GetCrossProduct(reference).GetNormalized(headLength * 0.5f);
			Vector3 side1 = direction.GetCrossProduct(side0).GetNormalized(headLength * 0.5f);
			const Vector3 baseCenter = to - direction * headLength;
			AppendLine(geometry, from, baseCenter, options);

			const uint32 base = static_cast<uint32>(geometry.positions.size());
			AppendVertex(geometry, to, options.color);
			AppendVertex(geometry, baseCenter + side0, options.color);
			AppendVertex(geometry, baseCenter + side1, options.color);
			AppendVertex(geometry, baseCenter - side0, options.color);
			AppendVertex(geometry, baseCenter - side1, options.color);

			const uint32 localIndices[] = {
				0, 1, 2,
				0, 2, 3,
				0, 3, 4,
				0, 4, 1
			};

			for(uint32 index : localIndices)
				geometry.indices.push_back(base + index);
		}

		static void AppendAABB(DebugGeometry &geometry, const Vector3 &origin, const RN::AABB &aabb, const DebugDrawOptions &options)
		{
			const Vector3 min = origin + aabb.minExtend;
			const Vector3 max = origin + aabb.maxExtend;

			const Vector3 corners[] = {
				Vector3(min.x, min.y, min.z),
				Vector3(max.x, min.y, min.z),
				Vector3(max.x, max.y, min.z),
				Vector3(min.x, max.y, min.z),
				Vector3(min.x, min.y, max.z),
				Vector3(max.x, min.y, max.z),
				Vector3(max.x, max.y, max.z),
				Vector3(min.x, max.y, max.z)
			};

			const uint8 edges[] = {
				0, 1, 1, 2, 2, 3, 3, 0,
				4, 5, 5, 6, 6, 7, 7, 4,
				0, 4, 1, 5, 2, 6, 3, 7
			};

			for(size_t i = 0; i < 24; i += 2)
				AppendLine(geometry, corners[edges[i]], corners[edges[i + 1]], options);
		}

		static void AppendSphere(DebugGeometry &geometry, const Vector3 &center, float radius, const DebugDrawOptions &options)
		{
			if(radius <= 0.0f)
			{
				AppendPoint(geometry, center, options);
				return;
			}

			const size_t segments = 32;
			for(size_t axis = 0; axis < 3; axis += 1)
			{
				for(size_t i = 0; i < segments; i += 1)
				{
					const float angle0 = (static_cast<float>(i) / static_cast<float>(segments)) * k::Pi * 2.0f;
					const float angle1 = (static_cast<float>(i + 1) / static_cast<float>(segments)) * k::Pi * 2.0f;
					const float sin0 = sin(angle0) * radius;
					const float cos0 = cos(angle0) * radius;
					const float sin1 = sin(angle1) * radius;
					const float cos1 = cos(angle1) * radius;

					Vector3 p0;
					Vector3 p1;
					if(axis == 0)
					{
						p0 = center + Vector3(0.0f, sin0, cos0);
						p1 = center + Vector3(0.0f, sin1, cos1);
					}
					else if(axis == 1)
					{
						p0 = center + Vector3(sin0, 0.0f, cos0);
						p1 = center + Vector3(sin1, 0.0f, cos1);
					}
					else
					{
						p0 = center + Vector3(sin0, cos0, 0.0f);
						p1 = center + Vector3(sin1, cos1, 0.0f);
					}

					AppendLine(geometry, p0, p1, options);
				}
			}
		}

		static void AppendCommand(DebugGeometry &depthGeometry, DebugGeometry &noDepthGeometry, const DebugCommand &command)
		{
			DebugGeometry &geometry = command.options.depthTest ? depthGeometry : noDepthGeometry;
			if(geometry.positions.empty())
			{
				geometry.origin = command.origin;
			}
			const Vector3 commandOrigin(command.origin - geometry.origin);

			switch(command.type)
			{
				case DebugCommandType::Point:
					AppendPoint(geometry, commandOrigin + command.from, command.options);
					break;

				case DebugCommandType::Line:
					AppendLine(geometry, commandOrigin + command.from, commandOrigin + command.to, command.options);
					break;

				case DebugCommandType::Ray:
					AppendRay(geometry, commandOrigin + command.from, commandOrigin + command.to, command.options);
					break;

				case DebugCommandType::AABB:
					AppendAABB(geometry, commandOrigin, command.aabb, command.options);
					break;

				case DebugCommandType::Sphere:
					AppendSphere(geometry, commandOrigin + command.from, command.radius, command.options);
					break;
			}
		}

		static Material *CreateMaterial(Renderer *renderer, Mesh *mesh, bool depthTest)
		{
			Shader::Options *shaderOptions = Shader::Options::WithMesh(mesh);
			Material *material = Material::WithShaders(renderer->GetDefaultShader(Shader::Type::Vertex, shaderOptions), renderer->GetDefaultShader(Shader::Type::Fragment, shaderOptions))->Retain();
			material->SetVertexShader(renderer->GetDefaultShader(Shader::Type::Vertex, shaderOptions, Shader::UsageHint::Depth), Shader::UsageHint::Depth);
			material->SetFragmentShader(renderer->GetDefaultShader(Shader::Type::Fragment, shaderOptions, Shader::UsageHint::Depth), Shader::UsageHint::Depth);
			material->SetVertexShader(renderer->GetDefaultShader(Shader::Type::Vertex, shaderOptions, Shader::UsageHint::Multiview), Shader::UsageHint::Multiview);
			material->SetFragmentShader(renderer->GetDefaultShader(Shader::Type::Fragment, shaderOptions, Shader::UsageHint::Multiview), Shader::UsageHint::Multiview);
			material->SetVertexShader(renderer->GetDefaultShader(Shader::Type::Vertex, shaderOptions, Shader::UsageHint::DepthMultiview), Shader::UsageHint::DepthMultiview);
			material->SetFragmentShader(renderer->GetDefaultShader(Shader::Type::Fragment, shaderOptions, Shader::UsageHint::DepthMultiview), Shader::UsageHint::DepthMultiview);
			material->SetAmbientColor(Color::White());
			material->SetDiffuseColor(Color::White());
			material->SetDepthWriteEnabled(false);
			material->SetDepthMode(depthTest ? DepthMode::Greater : DepthMode::Always);
			material->SetCullMode(CullMode::None);
			material->SetBlendOperation(BlendOperation::Add, BlendOperation::Add);
			material->SetBlendFactorSource(BlendFactor::SourceAlpha, BlendFactor::SourceAlpha);
			material->SetBlendFactorDestination(BlendFactor::OneMinusSourceAlpha, BlendFactor::OneMinusSourceAlpha);
			return material;
		}

		static Mesh *CreateMesh(const DebugGeometry &geometry)
		{
			Mesh *mesh = new Mesh({Mesh::VertexAttribute(Mesh::VertexAttribute::Feature::Vertices, PrimitiveType::Vector3),
								   Mesh::VertexAttribute(Mesh::VertexAttribute::Feature::Color0, PrimitiveType::Color),
								   Mesh::VertexAttribute(Mesh::VertexAttribute::Feature::Indices, PrimitiveType::Uint32)},
								  geometry.positions.size(), geometry.indices.size(), true);
			return mesh;
		}

		static void UpdateMesh(Mesh *mesh, const DebugGeometry &geometry)
		{
			mesh->BeginChanges();
			mesh->SetElementData(Mesh::VertexAttribute::Feature::Vertices, geometry.positions.data());
			mesh->SetElementData(Mesh::VertexAttribute::Feature::Color0, geometry.colors.data());
			mesh->SetElementData(Mesh::VertexAttribute::Feature::Indices, geometry.indices.data());
			mesh->EndChanges();
			mesh->CalculateBoundingVolumes();
		}

		static void UpdateBatch(Renderer *renderer, DebugBatch &batch, const DebugGeometry &geometry)
		{
			if(geometry.positions.empty() || geometry.indices.empty())
			{
				batch.entity->SetFlags(SceneNode::Flags::NoCulling | SceneNode::Flags::Hidden);
				return;
			}

			batch.entity->SetFlags(SceneNode::Flags::NoCulling);
			batch.entity->SetWorldPosition(geometry.origin);

			if(!batch.mesh || batch.vertexCount != geometry.positions.size() || batch.indexCount != geometry.indices.size())
			{
				SafeRelease(batch.mesh);
				batch.mesh = CreateMesh(geometry);
				batch.vertexCount = geometry.positions.size();
				batch.indexCount = geometry.indices.size();

				if(!batch.material)
					batch.material = CreateMaterial(renderer, batch.mesh, batch.depthTest);

				Model *model = new Model(batch.mesh, batch.material);
				batch.entity->SetModel(model);
				model->Release();
			}

			UpdateMesh(batch.mesh, geometry);
		}
	};

	DebugTextBackend::DebugTextBackend()
	{}

	DebugTextBackend::~DebugTextBackend()
	{}

	void DebugTextBackend::Clear()
	{}

	void DebugTextBackend::Update(float)
	{}

	void DebugTextBackend::WillRender(Renderer *, Camera *)
	{}

	void DebugTextBackend::DidRender(Renderer *)
	{}

	DebugRenderer::DebugRenderer() :
		_internals(new Internals())
	{
		_internals->depthBatch.depthTest = true;
		_internals->noDepthBatch.depthTest = false;
	}

	DebugRenderer::~DebugRenderer()
	{
		if(__defaultDebugRenderer == this)
			__defaultDebugRenderer = nullptr;

		UninstallFromScene();
		SafeRelease(_internals->depthBatch.mesh);
		SafeRelease(_internals->depthBatch.material);
		SafeRelease(_internals->noDepthBatch.mesh);
		SafeRelease(_internals->noDepthBatch.material);
		SafeRelease(_internals->textBackend);
		delete _internals;
	}

	void DebugRenderer::SetTextBackend(DebugTextBackend *backend)
	{
		if(_internals->textBackend == backend)
			return;

		if(_internals->textBackend)
			_internals->textBackend->Clear();

		SafeRelease(_internals->textBackend);
		_internals->textBackend = SafeRetain(backend);
	}

	void DebugRenderer::InstallInScene(Scene *scene)
	{
		if(_internals->installed)
			return;

		_internals->depthBatch.entity = new Entity();
		_internals->noDepthBatch.entity = new Entity();

		DebugBatch *batches[] = {&_internals->depthBatch, &_internals->noDepthBatch};
		for(DebugBatch *batch : batches)
		{
			batch->entity->SetUpdatePriority(SceneNode::UpdatePriority::UpdateNever);
			batch->entity->SetRenderPriority(SceneNode::RenderTransparent);
			batch->entity->SetFlags(SceneNode::Flags::NoCulling | SceneNode::Flags::Hidden);
			scene->AddNode(batch->entity);
		}

		_internals->installed = true;
	}

	void DebugRenderer::UninstallFromScene()
	{
		if(_internals->textBackend)
			_internals->textBackend->Clear();

		DebugBatch *batches[] = {&_internals->depthBatch, &_internals->noDepthBatch};
		for(DebugBatch *batch : batches)
		{
			if(!batch->entity)
				continue;

			if(batch->entity->GetSceneInfo())
				batch->entity->GetSceneInfo()->GetScene()->RemoveNode(batch->entity);

			batch->entity->Release();
			batch->entity = nullptr;
		}

		_internals->installed = false;
	}

	void DebugRenderer::Clear()
	{
		Lock();
		_internals->commands.clear();
		Unlock();

		if(_internals->textBackend)
			_internals->textBackend->Clear();
	}

	void DebugRenderer::AddCommand(uint8 type, const PositionType &origin, const Vector3 &from, const Vector3 &to, const RN::AABB &aabb, float radius, const DebugDrawOptions &options)
	{
		DebugCommand command;
		command.type = static_cast<DebugCommandType>(type);
		command.origin = origin;
		command.from = from;
		command.to = to;
		command.aabb = aabb;
		command.radius = radius;
		command.options = options;
		command.remaining = options.duration;
		command.rendered = false;

		Lock();
		_internals->commands.push_back(command);
		Unlock();
	}

	void DebugRenderer::DrawPoint(const Vector3 &position, const DebugDrawOptions &options)
	{
		AddCommand(static_cast<uint8>(DebugCommandType::Point), position, Vector3(), Vector3(), RN::AABB(), 0.0f, options);
	}

	void DebugRenderer::DrawLine(const Vector3 &from, const Vector3 &to, const DebugDrawOptions &options)
	{
		AddCommand(static_cast<uint8>(DebugCommandType::Line), from, Vector3(), to - from, RN::AABB(), 0.0f, options);
	}

	void DebugRenderer::DrawRay(const Vector3 &from, const Vector3 &to, const DebugDrawOptions &options)
	{
		AddCommand(static_cast<uint8>(DebugCommandType::Ray), from, Vector3(), to - from, RN::AABB(), 0.0f, options);
	}

	void DebugRenderer::DrawAABB(const RN::AABB &aabb, const DebugDrawOptions &options)
	{
		RN::AABB localAABB(aabb);
		localAABB.position = PositionType();
		AddCommand(static_cast<uint8>(DebugCommandType::AABB), aabb.position, Vector3(), Vector3(), localAABB, 0.0f, options);
	}

	void DebugRenderer::DrawSphere(const Vector3 &center, float radius, const DebugDrawOptions &options)
	{
		AddCommand(static_cast<uint8>(DebugCommandType::Sphere), center, Vector3(), Vector3(), RN::AABB(), radius, options);
	}

	void DebugRenderer::DrawSphere(const RN::Sphere &sphere, const DebugDrawOptions &options)
	{
		AddCommand(static_cast<uint8>(DebugCommandType::Sphere), sphere.position, sphere.offset, Vector3(), RN::AABB(), sphere.radius, options);
	}

	void DebugRenderer::DrawText(const Vector3 &position, const String *text, const DebugDrawOptions &options)
	{
		if(_internals->textBackend && text && GetParent())
			_internals->textBackend->DrawText(GetParent(), position, text, options);
	}

	void DebugRenderer::Update(float delta)
	{
		Lock();
		for(size_t i = 0; i < _internals->commands.size();)
		{
			DebugCommand &command = _internals->commands[i];
			if(command.remaining <= 0.0f || !command.rendered)
			{
				i += 1;
				continue;
			}

			command.remaining -= delta;
			if(command.remaining <= 0.0f)
			{
				_internals->commands.erase(_internals->commands.begin() + i);
				continue;
			}

			i += 1;
		}
		Unlock();

		if(_internals->textBackend)
			_internals->textBackend->Update(delta);
	}

	void DebugRenderer::WillRender(Renderer *renderer)
	{
		if(!_internals->installed && GetParent())
			InstallInScene(GetParent());

		Rebuild(renderer);
	}

	void DebugRenderer::DidRender(Renderer *renderer)
	{
		Lock();
		for(size_t i = 0; i < _internals->commands.size();)
		{
			if(_internals->commands[i].remaining <= 0.0f)
			{
				_internals->commands.erase(_internals->commands.begin() + i);
				continue;
			}

			_internals->commands[i].rendered = true;
			i += 1;
		}
		Unlock();

		if(_internals->textBackend)
			_internals->textBackend->DidRender(renderer);
	}

	void DebugRenderer::SubmitCameraPassAttachmentSnapshots(Renderer *renderer, Camera *camera, const SceneCameraPassContext &)
	{
		if(_internals->textBackend)
			_internals->textBackend->WillRender(renderer, camera);
	}

	void DebugRenderer::Rebuild(Renderer *renderer)
	{
		if(!_internals->installed || Renderer::IsHeadless())
			return;

		std::vector<DebugCommand> commands;
		Lock();
		commands = _internals->commands;
		Unlock();

		DebugGeometry depthGeometry;
		DebugGeometry noDepthGeometry;
		for(const DebugCommand &command : commands)
			DebugGeometryBuilder::AppendCommand(depthGeometry, noDepthGeometry, command);

		DebugGeometryBuilder::UpdateBatch(renderer, _internals->depthBatch, depthGeometry);
		DebugGeometryBuilder::UpdateBatch(renderer, _internals->noDepthBatch, noDepthGeometry);
	}

	void DebugDraw::AttachToScene(Scene *scene, DebugTextBackend *textBackend)
	{
		RN_ASSERT(scene, "DebugDraw::AttachToScene() requires a scene.");

		if(__defaultDebugRenderer && __defaultDebugRenderer->GetParent() == scene)
		{
			__defaultDebugRenderer->SetTextBackend(textBackend);
			return;
		}

		DebugRenderer *debugRenderer = new DebugRenderer();
		debugRenderer->SetTextBackend(textBackend);
		scene->AddAttachment(debugRenderer);
		debugRenderer->InstallInScene(scene);
		__defaultDebugRenderer = debugRenderer;
		debugRenderer->Release();
	}

	void DebugDraw::Clear()
	{
		if(__defaultDebugRenderer)
			__defaultDebugRenderer->Clear();
	}

	void DebugDraw::Point(const Vector3 &position, const DebugDrawOptions &options)
	{
		if(__defaultDebugRenderer)
			__defaultDebugRenderer->DrawPoint(position, options);
	}

	void DebugDraw::Line(const Vector3 &from, const Vector3 &to, const DebugDrawOptions &options)
	{
		if(__defaultDebugRenderer)
			__defaultDebugRenderer->DrawLine(from, to, options);
	}

	void DebugDraw::Ray(const Vector3 &origin, const Vector3 &direction, float length, const DebugDrawOptions &options)
	{
		if(__defaultDebugRenderer)
			__defaultDebugRenderer->DrawRay(origin, origin + direction.GetNormalized(length), options);
	}

	void DebugDraw::Ray(const Vector3 &origin, const Vector3 &direction, const DebugDrawOptions &options)
	{
		if(__defaultDebugRenderer)
			__defaultDebugRenderer->DrawRay(origin, origin + direction, options);
	}

	void DebugDraw::AABB(const RN::AABB &aabb, const DebugDrawOptions &options)
	{
		if(__defaultDebugRenderer)
			__defaultDebugRenderer->DrawAABB(aabb, options);
	}

	void DebugDraw::Sphere(const Vector3 &center, float radius, const DebugDrawOptions &options)
	{
		if(__defaultDebugRenderer)
			__defaultDebugRenderer->DrawSphere(center, radius, options);
	}

	void DebugDraw::Sphere(const RN::Sphere &sphere, const DebugDrawOptions &options)
	{
		if(__defaultDebugRenderer)
			__defaultDebugRenderer->DrawSphere(sphere, options);
	}

	void DebugDraw::Text(const Vector3 &position, const String *text, const DebugDrawOptions &options)
	{
		if(__defaultDebugRenderer)
			__defaultDebugRenderer->DrawText(position, text, options);
	}

	void DebugDraw::Text(const Vector3 &position, const char *text, const DebugDrawOptions &options)
	{
		if(!text)
			return;

		Text(position, String::WithString(text), options);
	}
} // namespace RN
