
import { Component, Index, Match, Show, Suspense, Switch, createMemo, createResource, createSignal } from "solid-js";
import { Button, ListGroup } from "solid-bootstrap";
import { MainModule } from "../wrapper/binary/wrapper"
import RemoteRendering from "./RemoteRendering";
import { CONDITION_INTERVALS, CONDITION_TECHNIQUES, REPITIONS, Technique } from "./Conditions";
import { useParams } from "@solidjs/router";
import { SERVER_URL } from "./RemoteRendering/Connection";
import { FrameData } from "./RemoteRendering/Renderer";

const CaptureReplay: Component<{ wrapper: MainModule }> = (props) => {
	const params = useParams();

	const filename = createMemo(() => {
		return `${params.technique}/${params.interval}/${params.run}-rendering.json`;
	});

	
	const [data, { mutate, refetch }] = createResource(filename, async () => {
		const response = await fetch(`http://${SERVER_URL}/files/${filename()}`, { mode: 'no-cors' });
		return response.json();
	});

	const [completed, setCompleted] = createSignal(false);

	return (
		<Switch>
			<Match when={data.loading}>
				<>Loading...</>
			</Match>
			<Match when={completed()}>
				<>Complete...</>
			</Match>
			<Match when={data()}>
				{
					data =>
					<RemoteRendering
						technique={CONDITION_TECHNIQUES.find(t => t.name === params.technique) as Technique}
						interval={parseInt(params.interval)}
						repetition={parseInt(params.run)}
						wrapper={props.wrapper}
						finished={() => setCompleted(true)}
						replayData={data()}
					/>
				}
			</Match>
		</Switch>
	);
};

export default CaptureReplay;