# Bakes project.env (repo URL + version: public, non-secret) into the build as BB_REPO_URL and
# BB_VERSION. See src/app/project_info.h.
import os

Import("env")  # noqa: F821

values = {}
with open(os.path.join(env.subst("$PROJECT_DIR"), "project.env")) as f:  # noqa: F821
    for line in f:
        line = line.strip()
        if line and not line.startswith("#") and "=" in line:
            key, value = line.split("=", 1)
            values[key.strip()] = value.strip()

env.Append(CPPDEFINES=[  # noqa: F821
    ("BB_REPO_URL", env.StringifyMacro(values.get("REPO_URL", ""))),  # noqa: F821
    ("BB_VERSION", env.StringifyMacro(values.get("VERSION", "0.0.0"))),  # noqa: F821
])
