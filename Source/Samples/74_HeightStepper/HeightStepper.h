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

#pragma once

#include <Urho3D/Scene/LogicComponent.h>

using namespace Urho3D;

namespace Urho3D
{
class Node;
class RigidBody;
}

//=============================================================================
//=============================================================================
class HeightStepper : public LogicComponent
{
    URHO3D_OBJECT(HeightStepper, LogicComponent);

public:
    HeightStepper(Context* context);
    
    static void RegisterObject(Context* context);
    
    virtual void Start();
    virtual void FixedUpdate(float timeStep);
    
    bool AddStepper(Node *charNode, const Vector3& walkerScale);
    bool IsSolid() const;

    bool StepperEnable(bool enable);
    bool GetStepperEnabled() const;

    // dbg
    void EnableStepper(bool enable);
    RigidBody* GetStepperRigidBody() const { return stepperBody_; }
    RigidBody* GetSolidRigidBody() const { return solidBody_; }

protected:
    void HandleNodeCollision(StringHash eventType, VariantMap& eventData);
    void SetSolid(bool solid);

protected:
    // attrib configurable
    float               minStepHeight_;
    float               maxStepHeight_;
    float               maxClimbAngle_;
    float               minStepNormal_;
    bool                applyImpulseToChar_;
    float               charStepUpDuration_;

    // character nodes
    WeakPtr<Node>       charNode_;

    // walker
    WeakPtr<Node>       stepperNode_;
    WeakPtr<Node>       solidNode_;
    WeakPtr<RigidBody>  stepperBody_;
    WeakPtr<RigidBody>  solidBody_;

    // public for debugging purpose
public:
    Vector3             stepHeightPos_;
    float               stepHeightLen_;
    Vector3             charPosAtHit_;
protected:

    Vector3             nodePlacementPos_;
    unsigned            stepState_;

private:
    enum StepState { StepState_Disabled, StepState_Idle, StepState_Solid };
};
