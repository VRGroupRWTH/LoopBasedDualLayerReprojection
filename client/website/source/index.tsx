import { Component, Show, createSignal } from "solid-js";
import { render } from "solid-js/web";
import { Button, Form } from "solid-bootstrap";
import { Route, Router } from "@solidjs/router";
import { glMatrix } from "gl-matrix";
import RemoteRendering, { WrapperModule, SessionConfig, DisplayType, SessionMode, load_wrapper_module } from "./remote_rendering";
import SceneBrowser from "./components/scene_browser"

import "../assets/styles.scss";

const App : Component<{wrapper : WrapperModule}> = (props) =>
{
    const [show, set_show] = createSignal(false);
    const [hallo, set_hallo] = createSignal(false);
    const [sele, set_sele] = createSignal("");

    glMatrix.setMatrixArrayType(Array);

    props.wrapper.default_mesh_settings()

    let config : SessionConfig =
    {
        mode: SessionMode.Capture,
        output_path: "/test2/",
        animation_file_name: "/test/animation_benchmark.json",
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
            <Route path="/" component=
            {() => 
            <div class="container py-5">

                <div>
                    <h4 class="border-bottom">Mesh Settings</h4>
                    <div class="row p-4 g-4">
                        <Form.Label class="col-2 col-form-label" for="mesh_method">Methode</Form.Label>
                        <div class="col-10">
                            <Form.Select id="mesh_method">
                                <option>Line-Based</option>
                                <option>Loop-Based</option>
                            </Form.Select>
                        </div>
                        <Form.Label class="col-2 col-form-label" for="mesh_method">Depth Base Threshold</Form.Label>
                        <div class="col-10">
                            <Form.Range></Form.Range>
                        </div>      
                    </div>
                </div>
                

                <Show when={!show()}>
                    <Button onClick={() => set_show(true)}>
                        Start
                    </Button>
                    <Button onClick={() => set_hallo(true)}>
                        Modal
                    </Button>
                    <SceneBrowser show={hallo} set_show={set_hallo} set_select={set_sele}></SceneBrowser>
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