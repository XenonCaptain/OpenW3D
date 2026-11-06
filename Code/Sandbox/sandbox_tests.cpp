#include "sandbox.h"
#include <cassert>
#include <cmath>
#include <limits>

using namespace openw3d::sandbox;

int main() {
    Policy policy;
    policy.max_props_per_player = 1;
    World world(SessionMode::Multiplayer, policy);
    assert(world.AllowPreset("Sandbox_Crate"));
    assert(!world.AllowPreset("Sandbox_Crate"));

    Request spawn;
    spawn.tool = Tool::Spawn;
    spawn.player = 1;
    spawn.preset = "Sandbox_Crate";
    auto first = world.Apply(spawn);
    assert(first.ok() && first.prop_id != 0);
    assert(world.PropCount() == 1);
    assert(world.PlayerPropCount(1) == 1);
    assert(world.Apply(spawn).error == Error::PropLimitReached);

    Request edit;
    edit.tool = Tool::Freeze;
    edit.target = first.prop_id;
    edit.player = 2;
    assert(world.Apply(edit).error == Error::NotAuthorized);
    edit.player = 1;
    assert(world.Apply(edit).ok() && world.Find(first.prop_id)->frozen);

    edit.tool = Tool::Move;
    edit.transform.position.x = std::numeric_limits<float>::quiet_NaN();
    assert(world.Apply(edit).error == Error::InvalidTransform);
    edit.transform.position.x = 10.0f;
    assert(world.Apply(edit).ok());
    assert(world.Find(first.prop_id)->transform.position.x == 10.0f);

    edit.tool = Tool::Duplicate;
    assert(world.Apply(edit).error == Error::PropLimitReached);
    edit.tool = Tool::Remove;
    assert(world.Apply(edit).ok());
    assert(world.Apply(edit).error == Error::PropNotFound);
    assert(world.PlayerPropCount(1) == 0);

    spawn.preset = "Unknown";
    assert(world.Apply(spawn).error == Error::InvalidPreset);
    world.DisallowPreset("Sandbox_Crate");
    spawn.preset = "Sandbox_Crate";
    assert(world.Apply(spawn).error == Error::InvalidPreset);
    world.Clear();
    assert(world.PropCount() == 0);
    return 0;
}
