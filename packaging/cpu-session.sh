#!/bin/bash
# Select a supported CPU governor for this port; never change frequency limits.
thing_cpu_root=/sys/devices/system/cpu/cpufreq
thing_cpu_policies=()
thing_cpu_governors=()
thing_cpu_begin() {
    [ "${THING_CPU_PERFORMANCE:-1}" = 1 ] || return 0
    local policy previous current
    for policy in "$thing_cpu_root"/policy*; do
        [ -r "$policy/scaling_available_governors" ] && [ -w "$policy/scaling_governor" ] || continue
        grep -qw performance "$policy/scaling_available_governors" || continue
        previous=$(cat "$policy/scaling_governor") || continue
        [[ "$previous" =~ ^[a-zA-Z0-9_]+$ ]] || continue
        [ "$previous" != performance ] || continue
        if printf 'performance\n' > "$policy/scaling_governor"; then
            current=$(cat "$policy/scaling_governor")
            if [ "$current" = performance ]; then
                thing_cpu_policies+=("$policy")
                thing_cpu_governors+=("$previous")
                echo "[cpu-session] ${policy##*/}: $previous -> performance; frequency limits unchanged"
            else
                printf '%s\n' "$previous" > "$policy/scaling_governor" || true
            fi
        fi
    done
    return 0
}
thing_cpu_restore() {
    local i policy current
    for ((i=${#thing_cpu_policies[@]}-1;i>=0;i--)); do
        policy=${thing_cpu_policies[i]}
        current=$(cat "$policy/scaling_governor" 2>/dev/null) || continue
        # Respect a later explicit setting made by firmware or the user.
        [ "$current" = performance ] || continue
        if printf '%s\n' "${thing_cpu_governors[i]}" > "$policy/scaling_governor"; then
            echo "[cpu-session] ${policy##*/}: restored ${thing_cpu_governors[i]}"
        fi
    done
    thing_cpu_policies=()
    thing_cpu_governors=()
    return 0
}
