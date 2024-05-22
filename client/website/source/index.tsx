import { Component } from "solid-js";
import { render } from "solid-js/web";
import { Route, Router } from "@solidjs/router";
import { glMatrix } from "gl-matrix";
import RemoteRendering, { WrapperModule, SessionConfig, SessionMode, load_wrapper_module } from "./remote_rendering";

import "../assets/styles.scss";

const App : Component<{wrapper : WrapperModule}> = (props) =>
{
    glMatrix.setMatrixArrayType(Array);

    let config : SessionConfig =
    {
        mode: SessionMode.Capture
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