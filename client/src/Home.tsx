import { Component, createEffect } from "solid-js";
import { SERVER_URL } from "./App";
import { useNavigate } from "@solidjs/router";

const STEPS = [
    "cond1-trial",
    "cond1-run",
    "cond1-questionnaire",
    "cond2-trial",
    "cond2-run",
    "cond2-questionnaire",
    "cond3-trial",
    "cond3-run",
    "cond3-questionnaire",
    "final",
];

const Home: Component<{ participantId: number }> = ({ participantId }) => {
    const navigate = useNavigate();

    createEffect(() => {
        // The following statement seems useless but is important to let the effect
        // run whenever the page changes.
        location.pathname;
        fetch(`${SERVER_URL}/progress?participantId=${participantId}`)
            .then(async response => {
                const json = await response.json();
                for (const step of STEPS) {
                    if (!json[step]) {
                        navigate(`/${step}`)
                        return;
                    }
                }
                navigate('/final');
            })
            .catch(console.error);
    });

    return <>Home</>;
}

export default Home;
