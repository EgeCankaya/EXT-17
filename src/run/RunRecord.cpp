#include "RunRecord.h"

#include "../common/Json.h"
#include "../common/Log.h"

#include <cstdio>

namespace ext17::run {

const char* toString(RunOutcome outcome) {
    switch (outcome) {
        case RunOutcome::Completed:           return "completed";
        case RunOutcome::Timeout:             return "timeout";
        case RunOutcome::InfrastructureError: return "infrastructure_error";
    }
    return "unknown";
}

std::string RunRecord::toJson() const {
    json::Writer w;
    w.beginObject();

    w.member("schema", schema);
    w.member("run_id", runId);
    w.member("outcome", std::string(toString(outcome)));
    if (outcome == RunOutcome::InfrastructureError) {
        w.beginObject("error");
        w.member("stage", errorStage);
        w.member("detail", errorDetail);
        w.endObject();
    } else {
        w.memberNull("error");
    }

    w.beginObject("scenario");
    w.member("name", scenario);
    w.member("model_name", modelName);
    w.endObject();

    // CR-EX-3: the predicate itself, its one-sentence statement, and the value it evaluated to.
    w.beginObject("stop_predicate");
    if (predicate) {
        w.member("kind", std::string(predicate->kindName()));
        w.member("statement", predicate->statement());
        w.member("frame_budget", predicate->frameBudgetValue());
    } else {
        w.member("kind", std::string("none"));
        w.member("statement", std::string(""));
        w.member("frame_budget", static_cast<std::uint64_t>(0));
    }
    w.member("wall_clock_participates", false);
    w.endObject();

    w.beginObject("stop_evaluation");
    w.member("satisfied", predicateSatisfied);
    w.member("observed_frame", evaluation.observedFrame);
    w.member("observed_sim_time_s", evaluation.observedSimTimeS, 6);
    w.member("observed_delta_s", evaluation.observedDeltaS, 5);
    w.endObject();

    // CR-EX-4: the timeout is a backstop and its own outcome, never the definition of the end.
    w.beginObject("run_timeout");
    w.member("timeout_ms", static_cast<std::int64_t>(runTimeoutMs));
    w.member("expired", runTimeoutExpired);
    w.member("role", std::string("backstop; never the definition of the end"));
    w.endObject();

    // CR-EX-2: every wait is an observed condition with a bounded, logged timeout. The
    // durations live in `diagnostics`, not here.
    w.beginArray("waits");
    for (const auto& wait : waits) {
        w.beginObject();
        w.member("what", wait.what);
        w.member("observed", wait.observed);
        w.member("timeout_ms", static_cast<std::int64_t>(wait.timeoutMs));
        w.member("frame_at_observation", wait.at.frame);
        w.endObject();
    }
    w.endArray();

    w.beginArray("processes");
    for (const auto& p : processes) {
        w.beginObject();
        w.member("role", p.role);
        w.member("exe", p.exePath);
        w.member("pid", static_cast<std::uint64_t>(p.pid));
        if (p.exitCode) {
            w.member("exit_code", static_cast<std::uint64_t>(*p.exitCode));
        } else {
            w.memberNull("exit_code");
        }
        w.member("started_by_campaign", p.startedByUs);
        w.member("exited_on_its_own", p.exitedOnItsOwn);
        w.member("graceful_stop_requested", p.gracefulStopRequested);
        // CR-EX-1: the campaign ends only handles it created, never an image name.
        w.member("terminated_by", std::string(p.terminatedByHandle ? "handle" : "none"));
        w.endObject();
    }
    w.endArray();

    w.beginObject("capture");
    w.member("recorder_attached", recorderAttached);
    w.member("path", capturePath);
    if (captureBytes) {
        w.member("bytes", *captureBytes);
    } else {
        w.memberNull("bytes");
    }
    w.beginArray("parts");
    for (const std::string& part : captureParts) { w.value(part); }
    w.endArray();
    w.member("total_bytes", captureTotalBytes);
    w.member("capture_max_bytes", captureMaxBytes);
    w.member("on_size_limit", onSizeLimit);
    w.member("read_back", captureRead);
    if (captureRead) {
        w.member("end_reason", captureEndReason);
        // The claim that matters, stated rather than inferable. False means the recorder stopped
        // at its byte bound before the run did: the capture is complete and valid and does not
        // cover the whole run.
        w.member("covers_whole_run", captureCoversWholeRun);
        w.member("conformant", captureConformant);
        w.member("samples", static_cast<std::int64_t>(captureSamples));
        w.member("segment_keys", static_cast<std::int64_t>(captureSegmentKeys));
        // Equal to segment_keys unless the capture rotated. A segment cut by a
        // rotation is one segment of the run appearing under two keys.
        w.member("run_segments", static_cast<std::int64_t>(captureRunSegments));
        if (captureDiagnostics.empty()) {
            w.memberNull("diagnostics");
        } else {
            w.member("diagnostics", captureDiagnostics);
        }
    }
    w.endObject();

    w.beginObject("environment");
    w.member("host_exe", hostExe);
    w.member("recorder_exe", recorderExe);
    w.member("sim_config_host", simConfigHost);
    w.member("sim_config_client", simConfigClient);
    w.member("model_path", modelPath);
    w.member("schema_file", schemaFile);
    w.member("n8ro_release", n8roRelease);
    w.member("path_prepend", pathPrepend);
    w.member("host_logger_copy", hostLoggerCopy);
    // The host appends to one shared log in the install tree; this is where this run's portion
    // of it began, and how many bytes of it the copy holds.
    w.member("host_logger_offset", hostLoggerOffset);
    if (hostLoggerBytes) { w.member("host_logger_bytes", *hostLoggerBytes); }
    else                 { w.memberNull("host_logger_bytes"); }
    w.endObject();

    w.beginObject("final_engine_state");
    w.member("state", finalSnapshot.state);
    w.member("frame", finalSnapshot.frame);
    w.member("sim_time_s", finalSnapshot.simTimeS, 6);
    w.member("delta_s", finalSnapshot.deltaS, 5);
    w.member("scenario_loaded", finalSnapshot.scenarioLoaded);
    w.endObject();

    // Everything below this line varies between identical runs and is excluded from every
    // comparison by construction. It is here because an operator needs it and CR-DET-2 needs
    // it kept out of the way.
    w.beginObject("diagnostics");
    w.member("note", std::string("Wall-clock measurements. Excluded from every comparison; "
                                 "no value here participates in any outcome or verdict."));
    w.beginArray("wait_elapsed_ms");
    for (const auto& wait : waits) {
        w.beginObject();
        w.member("what", wait.what);
        w.member("elapsed_ms", static_cast<std::int64_t>(wait.elapsedMs));
        w.endObject();
    }
    w.endArray();
    w.endObject();

    w.endObject();
    return w.str() + "\n";
}

bool writeRunRecord(const RunRecord& record, const std::string& path) {
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) {
        log::line("record", "could not open " + path + " for writing");
        return false;
    }
    const std::string text = record.toJson();
    const std::size_t written = std::fwrite(text.data(), 1, text.size(), f);
    std::fclose(f);
    if (written != text.size()) {
        log::line("record", "short write to " + path);
        return false;
    }
    log::line("record", path + " (" + std::string(toString(record.outcome)) + ")");
    return true;
}

} // namespace ext17::run
