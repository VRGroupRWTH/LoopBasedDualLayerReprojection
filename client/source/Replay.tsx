
import { Component, Index, Match, Show, Suspense, Switch, createMemo, createResource, createSignal } from "solid-js";
import { Button, ListGroup } from "solid-bootstrap";
import { MainModule } from "../wrapper/binary/wrapper"
import RemoteRendering from "./RemoteRendering";
import { CONDITION_INTERVALS, CONDITION_TECHNIQUES, REPITIONS, Technique } from "./Conditions";
import { useParams } from "@solidjs/router";
import { SERVER_URL } from "./RemoteRendering/Connection";
import { FrameData } from "./RemoteRendering/Renderer";
import { SessionConfig } from "./RemoteRendering/Session";

const Replay: Component<{ wrapper: MainModule, sessionType: "replay1" | "replay2" }> = (props) => {
	const params = useParams();

	const filename = createMemo(() => {
		return `${params.technique}/${params.interval}/${params.run}-rendering.json`;
	});

	
	const [data, { mutate, refetch }] = createResource(filename, async () => {
		const response = await fetch(`http://${SERVER_URL}/files/${filename()}`, { mode: 'no-cors' });
		return response.json();
	});

	const config = createMemo((): SessionConfig|undefined => {
		const replayData = data();

		if (!data) {
			return undefined;
		} else {
			return {
				technique: CONDITION_TECHNIQUES.find(t => t.name === params.technique) as Technique,
				interval: parseInt(params.interval),
				run: parseInt(params.run),
				sessionType: props.sessionType,
				replayData: replayData,
			};
		}
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
			<Match when={config()}>
				{
					config =>
						<RemoteRendering
							config={config()}
							wrapper={props.wrapper}
							finished={() => setCompleted(true)}
						/>
				}
			</Match>
		</Switch>
	);
};

export default Replay;