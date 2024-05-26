import { Component, Show, createSignal } from "solid-js";
import { render } from "solid-js/web";
import { Route, Router } from "@solidjs/router";
import { glMatrix } from "gl-matrix";
import RemoteRendering, { WrapperModule, SessionConfig, SessionMode, load_wrapper_module } from "./remote_rendering";
import { DisplayType } from "./remote_rendering/display";
import { Button } from "solid-bootstrap";

import "../assets/styles.scss";

const App : Component<{wrapper : WrapperModule}> = (props) =>
{
    const [show, set_show] = createSignal(false);

    glMatrix.setMatrixArrayType(Array);

    props.wrapper.default_mesh_settings()

    let config : SessionConfig =
    {
        mode: SessionMode.Capture,
        output_path: "/test",
        animation_file_name: "",
        resolution_width: 1024,
        resolution_height: 1024,
        layer_count: 2,
        render_request_rate: 500,
        mesh_generator: props.wrapper.MeshGeneratorType.MESH_GENERATOR_TYPE_LOOP,
        mesh_settings: props.wrapper.default_mesh_settings(),
        video_settings: props.wrapper.default_video_settings(),
        video_use_chroma_subsampling: true,
        scene_file_name: "/sponza/glTF/SponzaAnimation.fbx",
        scene_scale: 0.01,
        scene_exposure: 1.0,
        scene_indirect_intensity: 1.0,
        sky_file_name: "",
        sky_intensity: 1.0
    };

    function test()
    {

    }

    return (
        <Router>
            <Route path="/" component={() => 
            <div>
                <Show when={!show()}>
                    <Button onClick={() => set_show(true)}>
                        Start
                    </Button>
                </Show>
                <Show when={show()}>
                    <RemoteRendering wrapper={props.wrapper} config={config} on_close={test} preferred_display={DisplayType.AR}></RemoteRendering>
                </Show>
            </div>}
                 />
        </Router>
    );
};

load_wrapper_module().then(wrapper =>
{
    const app = document.getElementById("app");

    render(() => App({wrapper}), app!); 
});