#pragma once

#include "Vector.hpp"
#include <vector>
#include <memory>
#include <queue>
#include <limits>
#include <algorithm>

namespace CinemaKnot {

/**
 * @brief Search result structure returning matched corpus index and metric distance.
 */
struct SearchResult {
    uint32_t id{0};
    float distanceSquared{0.0f};

    float distance() const { return std::sqrt(distanceSquared); }

    bool operator<(const SearchResult& other) const {
        return distanceSquared < other.distanceSquared;
    }
};

/**
 * @brief Lightweight, dependency-free k-d tree for real-time spatial lookup.
 * Designed to compile on Raspberry Pi 4 (Linux) and ESP32 (Arduino / ESP-IDF).
 */
template <typename PayloadType = uint32_t>
class KDTree {
public:
    struct Entry {
        FeatureVector point;
        PayloadType payload;
    };

private:
    struct Node {
        Entry entry;
        size_t splitDim{0};
        std::unique_ptr<Node> left{nullptr};
        std::unique_ptr<Node> right{nullptr};

        Node(const Entry& e, size_t dim) : entry(e), splitDim(dim) {}
    };

    std::unique_ptr<Node> m_root{nullptr};
    size_t m_dimensions{0};
    size_t m_size{0};
    std::vector<float> m_weights;

public:
    KDTree() = default;

    size_t size() const { return m_size; }
    size_t dimensions() const { return m_dimensions; }
    bool empty() const { return m_size == 0; }

    void setDimensionWeights(const std::vector<float>& weights) {
        m_weights = weights;
    }

    /**
     * @brief Build balanced k-d tree from a collection of entries.
     */
    void build(std::vector<Entry>& entries) {
        m_root.reset();
        m_size = entries.size();
        if (entries.empty()) {
            m_dimensions = 0;
            return;
        }

        m_dimensions = entries.front().point.dim();
        std::vector<size_t> indices(entries.size());
        std::iota(indices.begin(), indices.end(), 0);

        m_root = buildRecursive(entries, indices, 0, indices.size(), 0);
    }

    /**
     * @brief Find single closest item (1-NN) in feature space.
     */
    bool findNearest(const FeatureVector& query, SearchResult& outResult, PayloadType* outPayload = nullptr) const {
        if (!m_root || m_size == 0) return false;

        float bestDistSq = std::numeric_limits<float>::max();
        const Node* bestNode = nullptr;

        nearestRecursive(m_root.get(), query, bestDistSq, bestNode);

        if (bestNode) {
            outResult.id = static_cast<uint32_t>(bestNode->entry.payload);
            outResult.distanceSquared = bestDistSq;
            if (outPayload) {
                *outPayload = bestNode->entry.payload;
            }
            return true;
        }
        return false;
    }

    /**
     * @brief Find K nearest neighbors (K-NN) for aleatoric/cluster navigation.
     */
    std::vector<SearchResult> findKNearest(const FeatureVector& query, size_t k) const {
        std::vector<SearchResult> results;
        if (!m_root || m_size == 0 || k == 0) return results;

        // Max-heap storing pairs of (distanceSquared, const Node*)
        using HeapItem = std::pair<float, const Node*>;
        auto comp = [](const HeapItem& a, const HeapItem& b) {
            return a.first < b.first;
        };
        std::priority_queue<HeapItem, std::vector<HeapItem>, decltype(comp)> heap(comp);

        kNearestRecursive(m_root.get(), query, k, heap);

        results.reserve(heap.size());
        while (!heap.empty()) {
            SearchResult res;
            res.distanceSquared = heap.top().first;
            res.id = static_cast<uint32_t>(heap.top().second->entry.payload);
            results.push_back(res);
            heap.pop();
        }
        std::reverse(results.begin(), results.end()); // Smallest distance first
        return results;
    }

private:
    std::unique_ptr<Node> buildRecursive(const std::vector<Entry>& entries,
                                        std::vector<size_t>& indices,
                                        size_t start, size_t end, size_t depth) {
        if (start >= end) return nullptr;

        size_t dim = depth % m_dimensions;
        size_t mid = start + (end - start) / 2;

        std::nth_element(indices.begin() + start,
                         indices.begin() + mid,
                         indices.begin() + end,
                         [&entries, dim](size_t a, size_t b) {
                             return entries[a].point[dim] < entries[b].point[dim];
                         });

        auto node = std::make_unique<Node>(entries[indices[mid]], dim);
        node->left = buildRecursive(entries, indices, start, mid, depth + 1);
        node->right = buildRecursive(entries, indices, mid + 1, end, depth + 1);
        return node;
    }

    void nearestRecursive(const Node* current, const FeatureVector& query,
                          float& bestDistSq, const Node*& bestNode) const {
        if (!current) return;

        float dSq = current->entry.point.distanceSquared(query, m_weights);
        if (dSq < bestDistSq) {
            bestDistSq = dSq;
            bestNode = current;
        }

        size_t dim = current->splitDim;
        float diff = query[dim] - current->entry.point[dim];
        float w = (m_weights.size() > dim) ? m_weights[dim] : 1.0f;
        float planeDistSq = w * (diff * diff);

        const Node* first = (diff <= 0.0f) ? current->left.get() : current->right.get();
        const Node* second = (diff <= 0.0f) ? current->right.get() : current->left.get();

        nearestRecursive(first, query, bestDistSq, bestNode);

        if (planeDistSq < bestDistSq) {
            nearestRecursive(second, query, bestDistSq, bestNode);
        }
    }

    template <typename Heap>
    void kNearestRecursive(const Node* current, const FeatureVector& query,
                           size_t k, Heap& heap) const {
        if (!current) return;

        float dSq = current->entry.point.distanceSquared(query, m_weights);
        if (heap.size() < k) {
            heap.push({dSq, current});
        } else if (dSq < heap.top().first) {
            heap.pop();
            heap.push({dSq, current});
        }

        size_t dim = current->splitDim;
        float diff = query[dim] - current->entry.point[dim];
        float w = (m_weights.size() > dim) ? m_weights[dim] : 1.0f;
        float planeDistSq = w * (diff * diff);

        const Node* first = (diff <= 0.0f) ? current->left.get() : current->right.get();
        const Node* second = (diff <= 0.0f) ? current->right.get() : current->left.get();

        kNearestRecursive(first, query, k, heap);

        if (heap.size() < k || planeDistSq < heap.top().first) {
            kNearestRecursive(second, query, k, heap);
        }
    }
};

} // namespace CinemaKnot
