//
// Copyright (c) 2008-2016 the Urho3D project.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#include <Urho3D/Core/Context.h>
#include <Urho3D/Scene/Scene.h>
#include <Urho3D/Scene/SceneEvents.h>
#include <Urho3D/IO/MemoryBuffer.h>
#include <Urho3D/Physics/PhysicsEvents.h>
#include <Urho3D/Physics/PhysicsWorld.h>
#include <Urho3D/Physics/RigidBody.h>
#include <Urho3D/Physics/CollisionShape.h>
#include <Urho3D/Graphics/StaticModel.h>
#include <Urho3D/Graphics/Model.h>
#include <Urho3D/Graphics/Material.h>
#include <Urho3D/Resource/ResourceCache.h>
#include <SDL/SDL_log.h>

#include "HeightStepper.h"
#include "CollisionLayer.h"

#include <Urho3D/DebugNew.h>
//=============================================================================
//=============================================================================
HeightStepper::HeightStepper(Context* context)
    : LogicComponent(context)
    , minStepHeight_(0.08f)
    , maxStepHeight_(0.32f)
    , maxClimbAngle_(40.0f)
    , minStepNormal_(0.5f)
    , applyImpulseToChar_(false)
    , charStepUpDuration_(0.25f)
    , nodePlacementPos_(Vector3(0.0f, -0.01f, -0.05f))
    , stepState_(StepState_Idle)
{
    SetUpdateEventMask(0);
}

void HeightStepper::RegisterObject(Context* context)
{
    context->RegisterFactory<HeightStepper>();
}

bool HeightStepper::AddStepper(Node *charNode, const Vector3& walkerScale)
{
    // character info
    charNode_ = charNode;

    // create
    stepperNode_ = node_->CreateChild("stepperNode");

    // reduce the step-up slope by moving the col shape a tad behind char's foot
    const float walkerZBackPosition = 0.48f;
    float boxHeight = 0.4f;

    stepperNode_->SetScale(walkerScale);
    stepperNode_->SetPosition(Vector3(0.0f, boxHeight * 0.5f + 0.15f, walkerScale.z_ * walkerZBackPosition));
    stepperNode_->SetRotation(Quaternion(-20.0f, Vector3(1,0,0)));

    // body 
    ResourceCache* cache = GetSubsystem<ResourceCache>();
    stepperBody_ = stepperNode_->CreateComponent<RigidBody>();
    stepperBody_->SetCollisionLayerAndMask(ColLayer_Walker, ColMask_Walker);
    stepperBody_->SetTrigger(true);
    stepperBody_->SetKinematic(true);
    stepperBody_->SetMass(1.0f);
    CollisionShape* colShape = stepperNode_->CreateComponent<CollisionShape>();

    colShape->SetBox(Vector3(1.0f, boxHeight, 1.0f));

    // solid
    solidNode_ = node_->CreateChild("solidNode");
    solidNode_->SetScale(walkerScale);
    solidNode_->SetPosition(Vector3(0.0f, 0.0f, walkerScale.z_ * walkerZBackPosition));
    solidBody_ = solidNode_->CreateComponent<RigidBody>();
    solidBody_->SetCollisionLayerAndMask(ColLayer_Static, ColMask_Walker);
    colShape = solidNode_->CreateComponent<CollisionShape>();
    colShape->SetTriangleMesh(cache->GetResource<Model>("Models/Plane.mdl"));

    SubscribeToEvent(stepperNode_, E_NODECOLLISION, URHO3D_HANDLER(HeightStepper, HandleNodeCollision));

    // walker active
    SetSolid(false);
    SetUpdateEventMask(USE_FIXEDUPDATE);

    return true;
}

void HeightStepper::Start()
{
}

void HeightStepper::SetSolid(bool solid)
{
    if (solid)
    {
        solidNode_->SetEnabled(true);
    }
    else
    {
        solidNode_->SetEnabled(false);
    }
}

bool HeightStepper::IsSolid() const
{
    return solidNode_->IsEnabled();
}

void HeightStepper::EnableStepper(bool enable)
{
    if (enable)
    {
        stepState_ = StepState_Idle;
    }
    else
    {
        stepState_ = StepState_Disabled;
    }

    // regardless of whatever state it was in, just start over
    solidNode_->SetEnabled(false);
}

bool HeightStepper::GetStepperEnabled() const
{
    return (stepState_ != StepState_Disabled);
}

void HeightStepper::FixedUpdate(float timeStep)
{
    // char data
    Vector3 charPos = charNode_->GetWorldPosition();
    Vector3 charDir = charNode_->GetWorldDirection();
    Quaternion charRot = charNode_->GetWorldRotation();

    // don't move the node until the char has stepped over it
    while (stepState_ == StepState_Solid)
    {
        Vector3 charToSH = (stepHeightPos_ - charPos);
        float distToChar = charToSH.Length();

        // wait for the char to move away from it
        if ((charPosAtHit_ - charPos).Length() < 0.5f)
        {
            return;
        }

        // if walking off in another direction
        if (charDir.DotProduct(charToSH.Normalized()) < 0.7f)
        {
            SetSolid(false);

            stepState_ = StepState_Idle;
            break;
        }

        // if the char is getting away from it
        if (distToChar > stepHeightLen_ * 1.2f)
        {
            SetSolid(false);
        
            stepState_ = StepState_Idle;
            break;
        }

        return;
    }

    // don't move the node until the char has walked over it
    stepHeightPos_ = charPos;
    node_->SetWorldPosition(charPos + charRot * nodePlacementPos_);
    node_->SetWorldRotation(charRot);
}

void HeightStepper::HandleNodeCollision(StringHash eventType, VariantMap& eventData)
{
    using namespace NodeCollision;

    // skip if not in the right condition
    if (stepState_ != StepState_Idle)
    {
        return;
    }

    MemoryBuffer contacts(eventData[P_CONTACTS].GetBuffer());
    Vector3 nodePos = node_->GetWorldPosition();
    Vector3 highestContactPt;
    float highestPt = 0.0f;

    while (!contacts.IsEof())
    {
        Vector3 contactPosition = contacts.ReadVector3();
        Vector3 contactNormal = contacts.ReadVector3();
        /*float contactDistance = */contacts.ReadFloat();
        /*float contactImpulse = */contacts.ReadFloat();

        // validate
        Vector3 segL = contactPosition - nodePos;
        float stepHeight = Vector3::UP.DotProduct(segL);

        // check climb angle
        if (Vector3::UP.DotProduct(segL.Normalized()) > maxClimbAngle_)
        {
            continue;
        }

        if (stepHeight < minStepHeight_ || stepHeight > maxStepHeight_)
        {
            continue;
        }

        // skip unsteppable contact normals
        if (Vector3::UP.DotProduct(contactNormal) < minStepNormal_)
        {
            continue;
        }

        if (stepHeight > highestPt)
        {
            highestPt = stepHeight;
            highestContactPt = contactPosition;
        }
    }

    if (highestPt > 0.0f)
    {
        // drop the height as to not let the char pop-up due to new collision under its feet
        highestContactPt.y_ -= 0.05f;
        Vector3 segL = highestContactPt - nodePos;
        float fwdSegL = node_->GetWorldDirection().DotProduct(segL);
        stepHeightPos_ = node_->GetWorldPosition() + node_->GetWorldRotation() * Vector3(0.0f, highestPt, fwdSegL);
        Vector3 charPos = charNode_->GetWorldPosition();
        charPosAtHit_ = charPos;
        stepHeightLen_ = (charPos - stepHeightPos_).Length();

        node_->LookAt(stepHeightPos_);

        // apply impulse to char
        if (applyImpulseToChar_)
        {
            charNode_->GetComponent<RigidBody>(true)->ApplyImpulse(Vector3::UP * highestPt / charStepUpDuration_);
        }

        SetSolid(true);

        stepState_ = StepState_Solid;
    }
}


