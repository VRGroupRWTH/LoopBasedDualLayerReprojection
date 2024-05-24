import { Component } from "solid-js";
import { render } from "solid-js/web";
import { Route, Router } from "@solidjs/router";
import { glMatrix } from "gl-matrix";
import RemoteRendering, { WrapperModule, SessionConfig, SessionMode, load_wrapper_module, MeshGeneratorType } from "./remote_rendering";

import "../assets/styles.scss";

const App : Component<{wrapper : WrapperModule}> = (props) =>
{
    glMatrix.setMatrixArrayType(Array);

    props.wrapper.default_mesh_settings()

    let config : SessionConfig =
    {
        mode: SessionMode.Capture,
        output_path: "/test",
        animation_file_name: "",
        resolution_width: 512,
        resolution_height: 512,
        layer_count: 1,
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
            <Route path="/" component={() => <RemoteRendering wrapper={props.wrapper} config={config} on_close={test}></RemoteRendering>} />
        </Router>
    );
};

load_wrapper_module().then(wrapper =>
{
    const app = document.getElementById("app");

    render(() => App({wrapper}), app!); 
});