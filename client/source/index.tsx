import { Component, Show, createSignal } from "solid-js";
import { render } from "solid-js/web";
import { Nav, Card } from "solid-bootstrap";
import Scene from "./scene";
import Settings from "./settings";

const root = document.getElementById("root");

const App : Component = () =>
{
    const [state, set_state] = createSignal("scene");

    return (
        <div class="p-4">
            <Card>
                <Card.Header>
                    <Nav variant="tabs">
                        <Nav.Item>
                            <Nav.Link active={state() == "scene"} disabled={state() != "scene"}>Scene</Nav.Link>
                        </Nav.Item>
                        <Nav.Item>
                            <Nav.Link active={state() == "settings"} disabled={state() != "settings"}>Settings</Nav.Link>
                        </Nav.Item>
                        <Nav.Item>
                            <Nav.Link active={state() == "capture"} disabled={state() != "capture"}>Capture</Nav.Link>
                        </Nav.Item>
                    </Nav>
                </Card.Header>
                <Card.Body>
                    <Show when={state() == "scene"}>
                        <Scene set_state={set_state}></Scene>
                    </Show>
                    <Show when={state() == "settings"}>
                        <Settings set_state={set_state}></Settings>
                    </Show>
                    <Show when={state() == "capture"}>
                        <Settings></Settings>
                    </Show>
                </Card.Body>
            </Card> 
        </div>
    );
};

render(() => App({}), root!);