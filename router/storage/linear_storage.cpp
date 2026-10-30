//
// Created by Aki on 9/14/26.
//

#include "linear_storage.h"

#include <utility>

// TODO: move to other file
namespace {
    std::string_view get_part(std::string_view& path) {
        while (!path.empty() && path.front() == '/') {
            path.remove_prefix(1);
        }
        auto pos = path.find('/');
        if (pos == std::string_view::npos) {
            auto part = path;
            path = {};
            return part;
        }
        auto part = path.substr(0, pos);
        path.remove_prefix(pos + 1);
        return part;
    }

    bool is_param(std::string_view part) {
        return part.size() > 2 && part.front() == '{' && part.back() == '}' &&
               part.substr(1, part.size() - 2).find_first_of("{}") == std::string_view::npos;
    }

    bool valid_pattern(std::string_view path) {
        if (path.empty() || path.find_first_of("?#") != std::string_view::npos) return false;
        while (true) {
            const auto part = get_part(path);
            if (part.empty()) return true;
            if (part.find_first_of("{}") != std::string_view::npos && !is_param(part))
                return false;
        }
    }

    bool same_pattern(std::string_view left, std::string_view right) {
        while (true) {
            const auto a = get_part(left);
            const auto b = get_part(right);
            if (a.empty() || b.empty()) return a.empty() && b.empty();
            if (a != b && !(is_param(a) && is_param(b))) return false;
        }
    }

    bool more_specific(std::string_view candidate, std::string_view current) {
        while (true) {
            const auto a = get_part(candidate);
            const auto b = get_part(current);
            if (a.empty() || b.empty()) return false;
            if (is_param(a) != is_param(b)) return !is_param(a);
        }
    }
}

bool capi::LinearStorage::parse_route(std::string_view path_sys, std::string_view path_us, RequestParams &params) {
    params = {};
    if (path_sys.empty() || path_us.empty()) {
        return false;
    }

    while (true) {
        auto p1 = get_part(path_sys);
        auto p2 = get_part(path_us);
        if (p1.empty() && p2.empty()) {
            return true;
        }
        if (p1.empty() || p2.empty()) {
            break;
        }
        if (is_param(p1)) {
            params.path_params.emplace_back(Param{
                p1.substr(1, p1.size() - 2),
                p2
            });
        } else if (p1 != p2) {
            break;
        }
    }

    params = {};
    return false;
}

bool capi::LinearStorage::add_route(const Route& route) {
    if (!route.handler || route.method == HttpMethod::NONE || !valid_pattern(route.path))
        return false;
    for (const auto& existing : routes_) {
        if (existing.method == route.method && same_pattern(existing.path, route.path))
            return false;
    }
    routes_.push_back(route);
    return true;
}

capi::RequestParams capi::LinearStorage::get_route(const Request &req) const {
    RequestParams params{};
    const Route* best = nullptr;
    for (const auto& route : routes_) {
        if (route.method != req.method) continue;
        if (best && !more_specific(route.path, best->path)) continue;
        RequestParams candidate{};
        if (parse_route(route.path, req.path, candidate)) {
            candidate.handler = &route.handler;
            params = std::move(candidate);
            best = &route;
        }
    }
    return params;
}
