# Source this file from any directory: . /path/to/big-r/env.sh
if [ -n "${ZSH_VERSION-}" ]; then
    big_r_env_dir="${${(%):-%x}:A:h}"
else
    big_r_env_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
fi
export IDF_PYTHON_ENV_PATH="${BIG_R_PYTHON_ENV_PATH:-$HOME/.espressif/python_env/idf6.0_py3.13_env}"
export ESP_MATTER_PATH="${big_r_env_dir}/../esp-matter"
. "${big_r_env_dir}/../esp-idf-v6.0.2/export.sh" || return
. "${ESP_MATTER_PATH}/export.sh"
unset big_r_env_dir
