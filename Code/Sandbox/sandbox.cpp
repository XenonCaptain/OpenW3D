/*
 * OpenW3D sandbox gameplay foundation.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "sandbox.h"

#include <cmath>
#include <limits>
#include <utility>

namespace openw3d::sandbox {
namespace {

bool ValidCoordinate(float value, float maximum) {
    return std::isfinite(value) && std::abs(value) <= maximum;
}

bool ValidRotation(float value) {
    return std::isfinite(value) && std::abs(value) <= 360000.0f;
}

} // namespace

World::World(SessionMode mode, Policy policy) : mode_(mode), policy_(policy) {}

bool World::AllowPreset(const std::string& name) {
    if (name.empty() || name.size() > 128) return false;
    return allowed_presets_.insert(name).second;
}

void World::DisallowPreset(const std::string& name) {
    allowed_presets_.erase(name);
}

bool World::IsValidTransform(const Transform& transform) const {
    const auto& p = transform.position;
    const auto& r = transform.rotation;
    return ValidCoordinate(p.x, policy_.max_coordinate)
        && ValidCoordinate(p.y, policy_.max_coordinate)
        && ValidCoordinate(p.z, policy_.max_coordinate)
        && ValidRotation(r.x) && ValidRotation(r.y) && ValidRotation(r.z);
}

bool World::CanEdit(const Request& request, const Prop& prop) const {
    return request.player == prop.owner || policy_.allow_edit_others;
}

Result World::Spawn(const Request& request) {
    if (!policy_.allow_spawn) return {Error::NotAuthorized, 0};
    if (!allowed_presets_.contains(request.preset)) return {Error::InvalidPreset, 0};
    if (!IsValidTransform(request.transform)) return {Error::InvalidTransform, 0};
    if (props_.size() >= policy_.max_props_total ||
        PlayerPropCount(request.player) >= policy_.max_props_per_player)
        return {Error::PropLimitReached, 0};
    if (next_id_ == 0 || next_id_ == std::numeric_limits<std::uint64_t>::max())
        return {Error::IdExhausted, 0};
    const auto id = next_id_++;
    props_.emplace(id, Prop{id, request.player, request.preset, request.transform, false});
    ++owner_counts_[request.player];
    return {Error::None, id};
}

Result World::Apply(const Request& request) {
    if (request.tool == Tool::Spawn) return Spawn(request);

    auto it = props_.find(request.target);
    if (it == props_.end()) return {Error::PropNotFound, 0};
    if (!CanEdit(request, it->second)) return {Error::NotAuthorized, 0};

    if (request.tool == Tool::Duplicate) {
        Request copy = request;
        copy.tool = Tool::Spawn;
        copy.preset = it->second.preset;
        return Spawn(copy);
    }

    if (request.tool == Tool::Remove) {
        const auto owner = it->second.owner;
        props_.erase(it);
        auto count = owner_counts_.find(owner);
        if (count != owner_counts_.end() && --count->second == 0)
            owner_counts_.erase(count);
        return {Error::None, request.target};
    }

    if (request.tool == Tool::Move || request.tool == Tool::Rotate) {
        if (!IsValidTransform(request.transform))
            return {Error::InvalidTransform, 0};
        if (request.tool == Tool::Move)
            it->second.transform.position = request.transform.position;
        else
            it->second.transform.rotation = request.transform.rotation;
    } else if (request.tool == Tool::Freeze) {
        it->second.frozen = true;
    } else if (request.tool == Tool::Unfreeze) {
        it->second.frozen = false;
    }
    return {Error::None, request.target};
}

const Prop* World::Find(std::uint64_t id) const {
    const auto it = props_.find(id);
    return it == props_.end() ? nullptr : &it->second;
}

std::size_t World::PropCount() const { return props_.size(); }

std::size_t World::PlayerPropCount(std::uint64_t player) const {
    const auto it = owner_counts_.find(player);
    return it == owner_counts_.end() ? 0 : it->second;
}

const std::unordered_map<std::uint64_t, Prop>& World::Props() const {
    return props_;
}

void World::Clear() {
    props_.clear();
    owner_counts_.clear();
    next_id_ = 1;
}

} // namespace openw3d::sandbox
