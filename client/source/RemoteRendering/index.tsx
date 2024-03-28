import { Component, createEffect, createSignal, onCleanup } from "solid-js";

import * as Wrapper from "../../wrapper/binary/wrapper"
import Session from "./Session";
import { TIME_BETWEEN_RUNS, Technique } from "../Conditions";

const RemoteRendering: Component<{
    wrapper: Wrapper.MainModule,
    interval: number,
    technique: Technique,
    repetition: number,
    finished: () => void,
}> = (props) => {
    const [session, setSession] = createSignal<Session>();
    const [canvas, setCanvas] = createSignal<HTMLCanvasElement>();

    createEffect(() => {
        const c = canvas();
        if (!c) {
            return;
        }

        const i = props.interval;
        const t = props.technique;
        const r = props.repetition;

        let to = setTimeout(() => {
            setSession(new Session(props.wrapper, c, t, i, r, props.finished));
            to = -1;
        }, TIME_BETWEEN_RUNS);

        onCleanup(() => {
            if (to >= 0) {
                clearTimeout(to);
            }
            session()?.close();
            setSession()
        });
    });

    return (
        <canvas tabIndex={0} ref={setCanvas} style={{ position: "absolute", left: 0, top: 0, "z-index": 100, width: "100%", height: "100%" }} />
    );
};

export default RemoteRendering;
