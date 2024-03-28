import { Component, Index, Show, Suspense, createResource, createSignal } from "solid-js";
import { Button, ListGroup } from "solid-bootstrap";
import { MainModule } from "../wrapper/binary/wrapper"
import RemoteRendering from "./RemoteRendering";
import { CONDITION_INTERVALS, CONDITION_TECHNIQUES, REPITIONS } from "./Conditions";

const Benchmark: Component<{ wrapper: MainModule }> = (props) => {
	const [techniqueIndex, setTechniqueIndex] = createSignal(0);
	const [intervalIndex, setIntervalIndex] = createSignal(0);
	const [repetition, setRepetition] = createSignal(0);
	const [completed, setCompleted] = createSignal(false);

	return (
		<Show when={!completed()} fallback={<b>Benchmarks complete</b>}>
			<RemoteRendering
				technique={CONDITION_TECHNIQUES[techniqueIndex()]}
				interval={CONDITION_INTERVALS[intervalIndex()]}
				repetition={repetition()}
				wrapper={props.wrapper}
				finished={() => {
					if (repetition() + 1 >= REPITIONS) {
						setRepetition(0);
						if (intervalIndex() + 1 >= CONDITION_INTERVALS.length) {
							setIntervalIndex(0);
							if (techniqueIndex() + 1 >= CONDITION_TECHNIQUES.length) {
								setCompleted(true);
							} else {
								setTechniqueIndex(techniqueIndex => techniqueIndex + 1);
							}
						} else {
							setIntervalIndex(intervalIndex => intervalIndex + 1);
						}
					} else {
						setRepetition(repetition => repetition + 1);
					}
				}}
			/>
		</Show>
	);
};

export default Benchmark;