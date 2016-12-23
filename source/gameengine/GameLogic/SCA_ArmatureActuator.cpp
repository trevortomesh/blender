/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file gameengine/GameLogic/SCA_ArmatureActuator.cpp
 *  \ingroup bgeconv
 */


#include "DNA_action_types.h"
#include "DNA_constraint_types.h"
#include "DNA_actuator_types.h"
#include "BKE_constraint.h"
#include "SCA_ArmatureActuator.h"
#include "BL_ArmatureObject.h"
#include "BLI_math.h"


// This class is the conversion of the Pose channel constraint.
// It makes a link between the pose constraint and the KX scene.
// The main purpose is to give access to the constraint target 
// to link it to a game object. 
// It also allows to activate/deactivate constraints during the game.
// Later it will also be possible to create constraint on the fly

SCA_ArmatureActuator::SCA_ArmatureActuator(SCA_IObject *obj,
                                           int type,
                                           const std::string &posechannel,
                                           const std::string &constraintname,
                                           KX_GameObject *targetobj,
                                           KX_GameObject *subtargetobj,
                                           float weight,
                                           float influence)
	: SCA_IActuator(obj, SCA_ACT_ARMATURE),
      m_constraint(NULL),
      m_gametarget(targetobj),
      m_gamesubtarget(subtargetobj),
      m_posechannel(posechannel),
      m_constraintname(constraintname),
      m_weight(weight),
      m_influence(influence),
      m_type(type)
{
	if (m_gametarget)
		m_gametarget->RegisterActuator(this);
	if (m_gamesubtarget)
		m_gamesubtarget->RegisterActuator(this);
	FindConstraint();
}

SCA_ArmatureActuator::~SCA_ArmatureActuator()
{
	if (m_gametarget)
		m_gametarget->UnregisterActuator(this);
	if (m_gamesubtarget)
		m_gamesubtarget->UnregisterActuator(this);
}

CValue *GetReplica() {
	SCA_ArmatureActuator *replica = new SCA_ArmatureActuator(*this);
	replica->ProcessReplica();
	return replica;
}

void SCA_ArmatureActuator::ProcessReplica()
{
	// the replica is tracking the same object => register it (this may be changed in Relnk())
	if (m_gametarget)
		m_gametarget->RegisterActuator(this);
	if (m_gamesubtarget)
		m_gamesubtarget->UnregisterActuator(this);
	SCA_IActuator::ProcessReplica();
}

void SCA_ArmatureActuator::ReParent(SCA_IObject *parent)
{
	SCA_IActuator::ReParent(parent);
	// must remap the constraint
	FindConstraint();
}

bool SCA_ArmatureActuator::UnlinkObject(SCA_IObject *clientobj)
{
	bool res=false;
	if (clientobj == m_gametarget) {
		// this object is being deleted, we cannot continue to track it.
		m_gametarget = NULL;
		res = true;
	}
	if (clientobj == m_gamesubtarget) {
		// this object is being deleted, we cannot continue to track it.
		m_gamesubtarget = NULL;
		res = true;
	}
	return res;
}

void SCA_ArmatureActuator::Relink(std::map<void *, void *> &obj_map)
{
	void *h_obj = obj_map[m_gametarget];
	if (h_obj) {
		if (m_gametarget)
			m_gametarget->UnregisterActuator(this);
		m_gametarget = (KX_GameObject*)h_obj;
		m_gametarget->RegisterActuator(this);
	}
	h_obj = obj_map[m_gamesubtarget];
	if (h_obj) {
		if (m_gamesubtarget)
			m_gamesubtarget->UnregisterActuator(this);
		m_gamesubtarget = (KX_GameObject *)h_obj;
		m_gamesubtarget->RegisterActuator(this);
	}
}

void SCA_ArmatureActuator::FindConstraint()
{
	m_constraint = NULL;

	if (m_gameobj->GetGameObjectType() == SCA_IObject::OBJ_ARMATURE) {
		BL_ArmatureObject *armobj = (BL_ArmatureObject *)m_gameobj;
		m_constraint = armobj->GetConstraint(m_posechannel, m_constraintname);
	}
}

bool SCA_ArmatureActuator::Update(double curtime, bool frame)
{
	// the only role of this actuator is to ensure that the armature pose will be evaluated
	bool result = false;
	bool bNegativeEvent = IsNegativeEvent();
	RemoveAllEvents();

	if (!bNegativeEvent) {
		BL_ArmatureObject *obj = (BL_ArmatureObject *)GetParent();
		switch (m_type) {
			case ACT_ARM_RUN:
			{
				result = true;
				obj->UpdateTimestep(curtime);
				break;
			}
			case ACT_ARM_ENABLE:
			{
				if (m_constraint)
					m_constraint->ClrConstraintFlag(CONSTRAINT_OFF);
				break;
			}
			case ACT_ARM_DISABLE:
			{
				if (m_constraint)
					m_constraint->SetConstraintFlag(CONSTRAINT_OFF);
				break;
			}
			case ACT_ARM_SETTARGET:
			{
				if (m_constraint) {
					m_constraint->SetTarget(m_gametarget);
					m_constraint->SetSubtarget(m_gamesubtarget);
				}
				break;
			}
			case ACT_ARM_SETWEIGHT:
			{
				if (m_constraint)
					m_constraint->SetWeight(m_weight);
				break;
			}
			case ACT_ARM_SETINFLUENCE:
			{
				if (m_constraint)
					m_constraint->SetInfluence(m_influence);
				break;
			}
		}
	}
	return result;
}

#ifdef WITH_PYTHON

/* ------------------------------------------------------------------------- */
/* Python Integration Hooks					                                 */
/* ------------------------------------------------------------------------- */

PyTypeObject SCA_ArmatureActuator::Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"SCA_ArmatureActuator",
	sizeof(PyObjectPlus_Proxy),
	0,
	py_base_dealloc,
	0,
	0,
	0,
	0,
	py_base_repr,
	0,0,0,0,0,0,0,0,0,
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
	0,0,0,0,0,0,0,
	Methods,
	0,
	0,
	&SCA_IActuator::Type,
	0,0,0,0,0,0,
	py_base_new
};


PyMethodDef SCA_ArmatureActuator::Methods[] = {
	{NULL,NULL} //Sentinel
};

PyAttributeDef SCA_ArmatureActuator::Attributes[] = {
	KX_PYATTRIBUTE_RO_FUNCTION("constraint", SCA_ArmatureActuator, pyattr_get_constraint),
	KX_PYATTRIBUTE_RW_FUNCTION("target", SCA_ArmatureActuator, pyattr_get_object, pyattr_set_object),
	KX_PYATTRIBUTE_RW_FUNCTION("subtarget", SCA_ArmatureActuator, pyattr_get_object, pyattr_set_object),
	KX_PYATTRIBUTE_FLOAT_RW("weight", 0.0f, 1.0f, SCA_ArmatureActuator, m_weight),
	KX_PYATTRIBUTE_FLOAT_RW("influence", 0.0f, 1.0f, SCA_ArmatureActuator, m_influence),
	KX_PYATTRIBUTE_INT_RW("type", 0, ACT_ARM_MAXTYPE, false, SCA_ArmatureActuator, m_type),
	KX_PYATTRIBUTE_NULL	//Sentinel
};

PyObject *SCA_ArmatureActuator::pyattr_get_object(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef)
{
	SCA_ArmatureActuator *actuator = static_cast<SCA_ArmatureActuator *>(self);
	KX_GameObject *target = (attrdef->m_name == "target") ? actuator->m_gametarget : actuator->m_gamesubtarget;
	if (!target)
		Py_RETURN_NONE;
	else
		return target->GetProxy();
}

int SCA_ArmatureActuator::pyattr_set_object(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	SCA_ArmatureActuator *actuator = static_cast<SCA_ArmatureActuator *>(self);
	KX_GameObject *target = (attrdef->m_name == "target") ? actuator->m_gametarget : actuator->m_gamesubtarget;
	KX_GameObject *gameobj;
		
	if (!ConvertPythonToGameObject(actuator->GetLogicManager(), value, &gameobj, true, "actuator.object = value: SCA_ArmatureActuator"))
		return PY_SET_ATTR_FAIL; // ConvertPythonToGameObject sets the error
		
	if (target != NULL)
		target->UnregisterActuator(actuator);

	target = gameobj;
		
	if (target)
		target->RegisterActuator(actuator);
		
	return PY_SET_ATTR_SUCCESS;
}

PyObject *SCA_ArmatureActuator::pyattr_get_constraint(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef)
{
	SCA_ArmatureActuator *actuator = static_cast<SCA_ArmatureActuator *>(self);
	BL_ArmatureConstraint *constraint = actuator->m_constraint;
	if (!constraint)
		Py_RETURN_NONE;
	else
		return constraint->GetProxy();
}

#endif // WITH_PYTHON
