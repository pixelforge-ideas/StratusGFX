#include "StratusEntity.h"
#include <glm/gtx/matrix_decompose.hpp>

namespace stratus {
    EntityPtr Entity::Create() {
        return EntityPtr(new Entity());
    }

    Entity::Entity() {
        _handle = EntityHandle::NextHandle();
        _refCount = std::make_shared<std::atomic<uint64_t>>(1);
        _RecalcTransform();
    }

    uint64_t Entity::GetRefCount() const {
        return _refCount->load();
    }

    void Entity::IncrRefCount() {
        _refCount->fetch_add(1);
    }

    void Entity::DecrRefCount() {
        _refCount->fetch_sub(1);
    }

    Entity::~Entity() {
        DecrRefCount();
    }

    void Entity::AddComponent(const EntityComponentPtr&) {
        throw std::runtime_error("Implement");
    }

    void Entity::RemoveComponent(const EntityComponentPtr&) {
        throw std::runtime_error("Implement");

    }

    void Entity::RemoveComponentByHandle(const EntityComponentHandle&) {
        throw std::runtime_error("Implement");

    }

    void Entity::RemoveAllComponents() {
        throw std::runtime_error("Implement");

    }

    const std::vector<EntityComponentPtr>& Entity::GetComponents() const {
        throw std::runtime_error("Implement");

    }

    const glm::vec3& Entity::GetLocalPosition() const {
        return _position;
    }

    void Entity::SetLocalPosition(const glm::vec3& pos) {
        _position = pos;
        _RecalcTransform();
    }

    const Rotation& Entity::GetLocalRotation() const {
        return _rotation;
    }

    void Entity::SetLocalRotation(const Rotation& rot) {
        _rotation = rot;
        _RecalcTransform();
    }

    const glm::vec3& Entity::GetLocalScale() const {
        return _scale;
    }

    void Entity::SetLocalScale(const glm::vec3& scale) {
        _scale = scale;
        _RecalcTransform();
    }

    void Entity::SetLocalPosRotScale(const glm::vec3& pos, const Rotation& rot, const glm::vec3& scale) {
        _position = pos;
        _rotation = rot;
        _scale = scale;
        _RecalcTransform();
    }

    const glm::vec3& Entity::GetWorldPosition() const {
        return _worldPosition;
    }

    const glm::mat4& Entity::GetWorldTransform() const {
        return _worldTransform;
    }

    const glm::mat4& Entity::GetLocalTransform() const {
        return _localTransform;
    }

    uint64_t Entity::GetEventMask() const {
        throw std::runtime_error("Implement");

    }

    void Entity::SendEvent(const EntityEvent&) {
        throw std::runtime_error("Implement");
    }

    void Entity::QueueEvent(const EntityEvent&) {
        throw std::runtime_error("Implement");

    }

    void Entity::GetQueuedEvents(std::vector<EntityEvent>& out) {
        throw std::runtime_error("Implement");

    }

    void Entity::ClearQueuedEvents() {
        throw std::runtime_error("Implement");
    }

    void Entity::_SetParent(EntityPtr parent) {
        _parent = parent;
    }

    EntityPtr Entity::GetParent() const {
        return _parent;
    }

    void Entity::AttachChild(EntityPtr child) {
        auto sharedThis = shared_from_this();
        if (child->GetParent() == sharedThis) return;
        else if (child->GetParent() != nullptr) {
            child->GetParent()->DetachChild(child);
        }

        _children.insert(child);
        child->_SetParent(sharedThis);
        child->_RecalcTransform();
    }

    void Entity::DetachChild(EntityPtr child) {
        if (_children.find(child) == _children.end()) return;
        _children.erase(child);
        child->_SetParent(nullptr);
    }

    const std::unordered_set<EntityPtr>& Entity::GetChildren() const {
        return _children;
    }

    EntityHandle Entity::GetHandle() const {
        return _handle;
    }

    void Entity::SetName(const std::string&) {
        throw std::runtime_error("Implement");
    }

    const std::string& Entity::GetName() const {
        throw std::runtime_error("Implement");
    }

    void Entity::_RecalcTransform() {
        // This is doing a lot of extra operations that can eventually be simplified. Plus it is using
        // too much storage compared to what it really should be.
        glm::mat4 scale(1.0f);
        glm::mat4 rotate(1.0f);
        glm::mat4 translate(1.0f);
        matScale(scale, _scale);
        matRotate(rotate, _rotation);
        matTranslate(translate, _position);
        _localTransform = translate * rotate * scale;
        _worldScale = scale;
        _worldRotate = rotate;
        _worldTranslate = translate;
        _worldTransform = _localTransform;
        if (_parent != nullptr) {
            _worldScale = _parent->_worldScale * scale;
            _worldRotate = _parent->_worldRotate * rotate;
            _worldTranslate = _parent->_worldTranslate * translate;
            _worldTransform = _parent->_worldTransform * _localTransform;
            _worldPosition = glm::vec3(_worldTransform[3].x, _worldTransform[3].y, _worldTransform[3].z);
        }

        if (_renderNode != nullptr) {
            _renderNode->SetWorldTransform(_worldTransform);
        }

        for (auto& child : _children) {
            child->_RecalcTransform();
        }
    }

    RenderNodePtr Entity::GetRenderNode() const {
        return _renderNode;
    }

    void Entity::SetRenderNode(const RenderNodePtr& node) {
        _renderNode = node;
        _renderNode->SetWorldTransform(_worldTransform);
        _renderNode->SetOwner(shared_from_this());
    }

    EntityPtr Entity::Copy(bool copyHandle) const {
        auto entity = Create();
        Copy(entity, copyHandle);
        return entity;
    }

    void Entity::Copy(EntityPtr& ptr, bool copyHandle) const {
        if (copyHandle) {
            ptr->_handle = _handle;
        }
        else {
            ptr->_handle = EntityHandle::NextHandle();
        }
        ptr->_refCount = _refCount;
        ptr->IncrRefCount();

        ptr->_position = GetLocalPosition();
        ptr->_rotation = GetLocalRotation();
        ptr->_scale = GetLocalScale();
        ptr->_worldPosition = GetWorldPosition();
        ptr->_localTransform = GetLocalTransform();
        ptr->_worldTransform = GetWorldTransform();

        if (_renderNode != nullptr) {
            ptr->_renderNode = _renderNode->Copy();
        }

        for (auto& child : _children) {
            auto childCopy = child->Copy(copyHandle);
            childCopy->_parent = ptr;
            ptr->_children.insert(childCopy);
        }
    }
}