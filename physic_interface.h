#pragma once

#include <functional>
#include <optional>
#include <variant>
#include <map>
#include "utils/base_system.h"
#include "utils/math/Transform.h"

struct IdType
{
	uint64_t value;
};

namespace Physic
{
	using Vector = DirectX::XMFLOAT2;

	struct Channels 
	{

	};

	struct Shape
	{
		Vector position;
		float radius;
	};

	struct PhysicProperties
	{
		float mass;
		float elasticity;
	};

	struct OnOverlap
	{
		std::function<void(IdType, IdType, Vector)> callback;
	};

	struct BodyState
	{
		Shape shape;
		Vector aggregate_force;
		float pressure;
	};

	struct TraceResult
	{
		IdType id;
		Vector hit;
	};

	struct PhysicTypes
	{
		bool bodys;
		bool volumes;
		bool obstacles;
	};

	enum class ForceType
	{
		Static,
		Impulse
	};

	struct Force
	{
		Vector normalized_direction;
		float amount;

		ForceType type;
	};

	struct SceneSnapshot
	{
		std::optional<BodyState> BodyState(IdType Id) const;
		std::optional<TraceResult> Trace(Vector start, Vector end,
			Channels channels, PhysicTypes types) const;
		void MultiTrace(Vector start, Vector end, Channels channels,
			PhysicTypes types, std::vector<TraceResult>& append_result) const;
	private:
		// shared state
	};

	class IPhysicSystem
	{
		// Scene
		virtual void UpdateSceneBoundaries(Vector min, Vector max);

		// Obstacles - immoveable
		virtual void AddObstacle(IdType id, Shape shape, Channels channels);
		virtual void ReleaseObstacle(IdType id);

		// Volumes - immaterial
		virtual void AddVolume(IdType id, Shape shape, Channels channels, OnOverlap overlap);
		virtual void UpdateVolumeShape(IdType id, Shape shape);
		virtual void ReleaseVolume(IdType id);

		// Bodys - regular moveable physical object
		virtual void AddBody(IdType id, Shape shape, Channels channels, 
			PhysicProperties prop, OnOverlap overlap, std::optional<Force> force);
		virtual void UpdateBodyShape(IdType id, Shape shape);
		virtual void UpdateBodyProperties(IdType id, PhysicProperties prop);
		virtual void ReleaseBody(IdType id);
		virtual void AddForces(std::multimap<IdType, Force> forces);

		// General
		virtual SceneSnapshot GetLastKnownSceneSnapshot() const;

		// ?
		virtual void NotifyNextUpdate(std::function<void()> callback);
		virtual void RegisterCallbackAfterUpdate(IdType id, std::function<void()> callback);
		virtual void UnregisterCallback(IdType Id);

		virtual ~IPhysicSystem() = default;
	};

	IPhysicSystem& GetSystem();
	IBaseSystem& CreateSystem();
}	