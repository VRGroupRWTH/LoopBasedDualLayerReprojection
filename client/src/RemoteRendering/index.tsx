import { Component, createSignal, onCleanup } from "solid-js";

import createARStreamingLib from "../../lib/build/ar-streaming-lib";
import Session from "./Session";

const RemoteRendering: Component<{
    finished: (time: number) => void,
    participantId: number,
    condition: number,
    trial?: boolean,
}> = ({ finished, participantId, condition, trial }) => {
    const [session, setSession] = createSignal<Session>();

    const setCanvas = (canvas: HTMLCanvasElement) => {
        createARStreamingLib().then(streamingLib => {
            const session = new Session(streamingLib, canvas, participantId, condition, trial || false);
            session.finished = finished;
            setSession(session)
        });
    };

    onCleanup(() => {
        session()?.close();
    });

    return (
        <canvas tabIndex={0} ref={setCanvas} style={{ position: "absolute", left: 0, top: 0, "z-index": 100, width: "100%", height: "100%" }} />
    );
};

export default RemoteRendering;
