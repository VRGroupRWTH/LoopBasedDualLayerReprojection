import { Component, Index, Show, Suspense, createMemo, createResource, createSignal } from "solid-js";
import { Button, ListGroup } from "solid-bootstrap";
import { MainModule } from "../wrapper/binary/wrapper"
import RemoteRendering from "./RemoteRendering";
import { CONDITION_INTERVALS, CONDITION_TECHNIQUES, REPITIONS, Technique } from "./Conditions";
import { SessionConfig } from "./RemoteRendering/Session";
import { useParams } from "@solidjs/router";

const BenchmarkSuite: Component<{ wrapper: MainModule }> = (props) => {
	const params = useParams();

	const techniques = createMemo(() => params.technique === undefined || params.technique === "all" ? CONDITION_TECHNIQUES : CONDITION_TECHNIQUES.filter(t => t.name === params.technique));
	const intervals = createMemo(() => params.interval === undefined || params.interval === "all" ? CONDITION_INTERVALS : [parseInt(params.interval)]);
	const numberOfRuns = createMemo(() => params.numberOfRuns === undefined || params.numberOfRuns === "all" ? REPITIONS : parseInt(params.numberOfRuns));

	const [techniqueIndex, setTechniqueIndex] = createSignal(0);
	const [intervalIndex, setIntervalIndex] = createSignal(0);
	const [run, setRun] = createSignal(0);
	const nextBenchmark = () => {
		if (run() + 1 >= numberOfRuns()) {
			setRun(0);
			if (intervalIndex() + 1 >= intervals().length) {
				setIntervalIndex(0);
				if (techniqueIndex() + 1 >= techniques().length) {
					setCompleted(true);
				} else {
					setTechniqueIndex(techniqueIndex => techniqueIndex + 1);
				}
			} else {
				setIntervalIndex(intervalIndex => intervalIndex + 1);
			}
		} else {
			setRun(run => run + 1);
		}
	}
	const config = createMemo((): SessionConfig => {
		return {
			interval: intervals()[intervalIndex()],
			technique: techniques()[techniqueIndex()],
			run: run(),
			sessionType: "benchmark"
		}
	});
	const [completed, setCompleted] = createSignal(false);

	return (
		<Show when={!completed()} fallback={<b>Benchmarks complete</b>}>
			<RemoteRendering
				config={config()}
				wrapper={props.wrapper}
				finished={nextBenchmark}
			/>
		</Show>
	);
};

export default BenchmarkSuite;