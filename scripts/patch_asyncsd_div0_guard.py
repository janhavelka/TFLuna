from pathlib import Path

Import("env")  # type: ignore # pylint: disable=undefined-variable


MARKER = "// TFLUNACTRL_DIV0_GUARD_PATCH"
LEGACY_MARKER = "// " + "CO2" + "CONTROL_DIV0_GUARD_PATCH"
REQUIRED_SNIPPETS = (
    "static inline uint8_t advanceRingIndex(uint8_t idx, uint8_t depth) {",
    "advanceRingIndex(st->reqTail, st->reqDepth)",
    "advanceRingIndex(st->reqHead, st->reqDepth)",
    "advanceRingIndex(st->resTail, st->resDepth)",
    "advanceRingIndex(searchIdx, st->resDepth)",
)
DISALLOWED_SNIPPETS = (
    "static_cast<uint8_t>((st->reqTail + 1) % st->reqDepth)",
    "static_cast<uint8_t>((idx + 1) % _internal->resDepth)",
    "static_cast<uint8_t>((searchIdx + 1) % st->resDepth)",
    "static_cast<uint8_t>((st->resTail + 1) % st->resDepth)",
    "static_cast<uint8_t>((idx + 1) % st->reqDepth)",
    "static_cast<uint8_t>((st->reqHead + 1) % st->reqDepth)",
)


def _validate_patched_text(text: str, path: Path) -> None:
    missing = [snippet for snippet in REQUIRED_SNIPPETS if snippet not in text]
    present_disallowed = [snippet for snippet in DISALLOWED_SNIPPETS if snippet in text]
    has_marker = (MARKER in text) or (LEGACY_MARKER in text)
    if missing or present_disallowed or not has_marker:
        details = []
        if missing:
            details.append("missing required snippets: " + ", ".join(missing))
        if present_disallowed:
            details.append("found disallowed snippets: " + ", ".join(present_disallowed))
        if not has_marker:
            details.append("missing patch marker")
        raise RuntimeError(f"AsyncSD div0 guard patch validation failed for {path}: {'; '.join(details)}")


def _patch_asyncsd_cpp(path: Path) -> bool:
    text = path.read_text(encoding="utf-8")
    changed = False
    if LEGACY_MARKER in text and MARKER not in text:
        text = text.replace(LEGACY_MARKER, MARKER)
        changed = True
    already_patched = (MARKER in text) or (LEGACY_MARKER in text)

    if not already_patched:
        helper_anchor = "static uint32_t selectTimeoutMs(const SdCardConfig& cfg, RequestType type) {"
        helper_block = (
            "static inline uint8_t advanceRingIndex(uint8_t idx, uint8_t depth) {\n"
            "  return (depth == 0U) ? 0U : static_cast<uint8_t>((idx + 1U) % depth);\n"
            "}\n\n"
        )
        if helper_anchor in text:
            text = text.replace(helper_anchor, helper_block + helper_anchor, 1)
            changed = True

        replacements = {
            "static_cast<uint8_t>((st->reqTail + 1) % st->reqDepth)": "advanceRingIndex(st->reqTail, st->reqDepth)",
            "static_cast<uint8_t>((idx + 1) % _internal->resDepth)": "advanceRingIndex(idx, _internal->resDepth)",
            "static_cast<uint8_t>((searchIdx + 1) % st->resDepth)": "advanceRingIndex(searchIdx, st->resDepth)",
            "static_cast<uint8_t>((st->resTail + 1) % st->resDepth)": "advanceRingIndex(st->resTail, st->resDepth)",
            "static_cast<uint8_t>((idx + 1) % st->reqDepth)": "advanceRingIndex(idx, st->reqDepth)",
            "static_cast<uint8_t>((st->reqHead + 1) % st->reqDepth)": "advanceRingIndex(st->reqHead, st->reqDepth)",
        }
        for old, new in replacements.items():
            if old in text:
                text = text.replace(old, new)
                changed = True

        worker_guard_anchor = (
            "static void workerStepCore(Internal* st, const SdCardConfig& cfg, uint32_t budgetUs) {\n"
            "  if (!st || !st->initialized) {\n"
            "    return;\n"
            "  }\n\n"
            "  const uint32_t startUs = micros();\n"
        )
        worker_guard_block = (
            "static void workerStepCore(Internal* st, const SdCardConfig& cfg, uint32_t budgetUs) {\n"
            "  if (!st || !st->initialized) {\n"
            "    return;\n"
            "  }\n\n"
            f"  {MARKER}\n"
            "  if (st->reqDepth == 0U || st->resDepth == 0U ||\n"
            "      st->reqQueue == nullptr || st->resQueue == nullptr) {\n"
            "    setLastError(st, ErrorCode::Fault, Operation::None, 0, nullptr, 0, 0);\n"
            "    setStatus(st, SdStatus::Fault);\n"
            "    st->pendingAutoMount = false;\n"
            "    st->pendingAutoUnmount = false;\n"
            "    st->reqHead = 0U;\n"
            "    st->reqTail = 0U;\n"
            "    st->reqCount = 0U;\n"
            "    st->resHead = 0U;\n"
            "    st->resTail = 0U;\n"
            "    st->resCount = 0U;\n"
            "    st->health.queueDepthRequests.store(0, std::memory_order_relaxed);\n"
            "    st->health.queueDepthResults.store(0, std::memory_order_relaxed);\n"
            "    recordFailure(st);\n"
            "    recordProgress(st, millis());\n"
            "    return;\n"
            "  }\n\n"
            "  const uint32_t startUs = micros();\n"
        )
        if worker_guard_anchor in text:
            text = text.replace(worker_guard_anchor, worker_guard_block, 1)
            changed = True

    _validate_patched_text(text, path)
    if changed:
        path.write_text(text, encoding="utf-8", newline="\n")
    return changed


def _patch_for_env() -> None:
    env_name = env.get("PIOENV")
    if not env_name:
        return
    libdeps_dir = Path(env.subst("$PROJECT_LIBDEPS_DIR"))
    target = libdeps_dir / env_name / "AsyncSD" / "src" / "AsyncSD.cpp"
    if not target.exists():
        return
    patched = _patch_asyncsd_cpp(target)
    if patched:
        print(f"[patch_asyncsd_div0_guard] patched {target}")
    else:
        print(f"[patch_asyncsd_div0_guard] no changes for {target}")


_patch_for_env()
