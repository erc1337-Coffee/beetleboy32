#pragma once

// From project.env at the repo root, baked in at build time by scripts/project_info.py.
namespace project {
constexpr const char *REPO_URL = BB_REPO_URL;
constexpr const char *VERSION = BB_VERSION;
}  // namespace project
