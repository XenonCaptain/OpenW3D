/*
 * OpenW3D sandbox gameplay foundation.
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This module deliberately does not include engine headers. Integration
 * adapters are responsible for converting validated requests into W3D objects.
 */
#ifndef OPENW3D_SANDBOX_H
#define OPENW3D_SANDBOX_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace openw3d::sandbox {

enum class SessionMode { SinglePlayer, Multiplayer };
enum class Tool { Spawn, Remove, Move, Rotate, Freeze, Unfreeze, Duplicate };

struct Vector3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct Transform {
    Vector3 position;
    Vector3 rotation;
};

struct Prop {
    std::uint64_t id = 0;
    std::uint64_t owner = 0;
    std::string preset;
    Transform transform;
    bool frozen = false;
};

struct Request {
    Tool tool = Tool::Spawn;
    std::uint64_t player = 0;
    std::uint64_t target = 0;
    std::string preset;
    Transform transform;
};

struct Policy {
    std::size_t max_props_per_player = 200;
    std::size_t max_props_total = 2000;
    float max_coordinate = 100000.0f;
    bool allow_edit_others = false;
    bool allow_spawn = true;
};

enum class Error {
    None,
    NotAuthorized,
    InvalidPreset,
    InvalidTransform,
    PropLimitReached,
    PropNotFound,
    IdExhausted,
};

struct Result {
    Error error = Error::None;
    std::uint64_t prop_id = 0;
    bool ok() const { return error == Error::None; }
};

// Server-authoritative state model: every tool action must pass through Apply.
// This is *not* an engine/network integration by itself.
class World {
public:
    explicit World(SessionMode mode, Policy policy = {});

    bool AllowPreset(const std::string& name);
    void DisallowPreset(const std::string& name);
    Result Apply(const Request& request);
    const Prop* Find(std::uint64_t id) const;
    std::size_t PropCount() const;
    std::size_t PlayerPropCount(std::uint64_t player) const;
    const std::unordered_map<std::uint64_t, Prop>& Props() const;
    void Clear();

    SessionMode Mode() const { return mode_; }
    const Policy& GetPolicy() const { return policy_; }

private:
    bool IsValidTransform(const Transform& transform) const;
    bool CanEdit(const Request& request, const Prop& prop) const;
    Result Spawn(const Request& request);

    SessionMode mode_;
    Policy policy_;
    std::uint64_t next_id_ = 1;
    std::unordered_set<std::string> allowed_presets_;
    std::unordered_map<std::uint64_t, Prop> props_;
    std::unordered_map<std::uint64_t, std::size_t> owner_counts_;
};

} // namespace openw3d::sandbox

#endif
