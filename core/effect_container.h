// avs_lib - Portable Advanced Visualization Studio library
// Based on Advanced Visualization Studio by Nullsoft, Inc.
// AVS Copyright (C) 2005 Nullsoft, Inc.
// C++20 port Copyright (C) 2025 Tim Redfern
// Licensed under MIT License - see LICENSE file in repository root

#pragma once

#include "effect_base.h"
#include <vector>
#include <memory>

namespace avs {

// Base class for any effect that can contain child effects
// This provides the tree structure for nested effect lists
class EffectContainer : public EffectBase {
public:
    virtual ~EffectContainer() = default;

    // Child management
    virtual void add_child(std::unique_ptr<EffectBase> effect) {
        children_.push_back(std::move(effect));
    }

    virtual void insert_child(size_t index, std::unique_ptr<EffectBase> effect) {
        if (index >= children_.size()) {
            children_.push_back(std::move(effect));
        } else {
            children_.insert(children_.begin() + index, std::move(effect));
        }
    }

    virtual void remove_child(size_t index) {
        if (index < children_.size()) {
            children_.erase(children_.begin() + index);
        }
    }

    // Take ownership of a child (for moving between containers)
    virtual std::unique_ptr<EffectBase> take_child(size_t index) {
        if (index >= children_.size()) {
            return nullptr;
        }
        auto effect = std::move(children_[index]);
        children_.erase(children_.begin() + index);
        return effect;
    }

    virtual EffectBase* get_child(size_t index) {
        if (index >= children_.size()) {
            return nullptr;
        }
        return children_[index].get();
    }

    virtual const EffectBase* get_child(size_t index) const {
        if (index >= children_.size()) {
            return nullptr;
        }
        return children_[index].get();
    }

    virtual size_t child_count() const {
        return children_.size();
    }

    // Swap two children (for reordering)
    virtual void swap_children(size_t index_a, size_t index_b) {
        if (index_a < children_.size() && index_b < children_.size()) {
            std::swap(children_[index_a], children_[index_b]);
        }
    }

    // Move a child up or down in the list
    virtual bool move_child_up(size_t index) {
        if (index > 0 && index < children_.size()) {
            std::swap(children_[index], children_[index - 1]);
            return true;
        }
        return false;
    }

    virtual bool move_child_down(size_t index) {
        if (index + 1 < children_.size()) {
            std::swap(children_[index], children_[index + 1]);
            return true;
        }
        return false;
    }

    // Find the index of a child effect
    virtual int find_child_index(const EffectBase* effect) const {
        for (size_t i = 0; i < children_.size(); i++) {
            if (children_[i].get() == effect) {
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    // Find parent container of an effect (searches from this container down)
    EffectContainer* find_parent_of(EffectBase* effect) {
        if (!effect) return nullptr;

        for (size_t i = 0; i < children_.size(); i++) {
            if (children_[i].get() == effect) {
                return this;
            }

            auto* child_container = dynamic_cast<EffectContainer*>(children_[i].get());
            if (child_container) {
                auto* found = child_container->find_parent_of(effect);
                if (found) return found;
            }
        }

        return nullptr;
    }

    // Get siblings by effect pointer (finds parent internally)
    EffectBase* get_sibling_after(EffectBase* effect) {
        EffectContainer* parent = find_parent_of(effect);
        if (!parent) return nullptr;

        int index = parent->find_child_index(effect);
        if (index < 0 || static_cast<size_t>(index + 1) >= parent->children_.size()) {
            return nullptr;
        }
        return parent->children_[index + 1].get();
    }

    EffectBase* get_sibling_before(EffectBase* effect) {
        EffectContainer* parent = find_parent_of(effect);
        if (!parent) return nullptr;

        int index = parent->find_child_index(effect);
        if (index <= 0) return nullptr;
        return parent->children_[index - 1].get();
    }

    // Remove an effect from anywhere in the tree
    bool remove_effect(EffectBase* effect) {
        if (!effect) return false;

        EffectContainer* parent = find_parent_of(effect);
        if (!parent) return false;

        int index = parent->find_child_index(effect);
        if (index < 0) return false;

        parent->remove_child(static_cast<size_t>(index));
        return true;
    }

    // UI expand/collapse state (for tree view)
    bool is_expanded() const { return expanded_; }
    void set_expanded(bool expanded) { expanded_ = expanded; }

protected:
    std::vector<std::unique_ptr<EffectBase>> children_;
    bool expanded_ = true;
};

} // namespace avs
