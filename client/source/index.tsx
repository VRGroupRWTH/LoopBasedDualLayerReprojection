import { Component, Show, createSignal } from "solid-js";
import { render } from "solid-js/web";
import { Nav, Card } from "solid-bootstrap";
import Scene from "./scene";
import {Settings, SettingsType} from "./settings";
import * as Wrapper from "../wrapper/binary/wrapper"

const App : Component<{wrapper : Wrapper.MainModule}> = (props) =>
{
    const default_settings : SettingsType =
    {
        resolution: 1024,
        update_rate: 1,
        scene_scale: 1.0,
        scene_exposure: 1.0,
        scene_indirect_intensity: 1.0,
        mesh_generator: props.wrapper.MeshGeneratorType.MESH_GENERATOR_TYPE_LINE,
        mesh_settings: props.wrapper.default_mesh_settings(),
        video_settings: props.wrapper.default_video_settings()
    };

    const [state, set_state] = createSignal("scene");
    const [scene, set_scene] = createSignal("");
    const [settings, set_settings] = createSignal(default_settings);
    const [capture, set_capture] = createSignal("");
    
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

Wrapper.default().then(wrapper =>
{
    const root = document.getElementById("root");

    render(() => App({wrapper}), root!); 
});