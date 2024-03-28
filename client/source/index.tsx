import { Component, Show, createSignal } from "solid-js";
import { render } from "solid-js/web";
import { Nav, Card } from "solid-bootstrap";
import Scene from "./scene";
import {Settings, SettingsType} from "./settings";
import * as Wrapper from "../wrapper/binary/wrapper"
import { glMatrix } from "gl-matrix";
import { Route, Router } from "@solidjs/router";
import Replay from "./Replay";
import BenchmarkSuite from "./BenchmarkSuite";


const App : Component<{wrapper : Wrapper.MainModule}> = (props) =>
{
    glMatrix.setMatrixArrayType(Array);

    return (
        <Router>
            <Route path="/benchmarks" component={() => <BenchmarkSuite wrapper={props.wrapper} />} />
            <Route path="/benchmarks/:technique" component={() => <BenchmarkSuite wrapper={props.wrapper} />} />
            <Route path="/benchmarks/:technique/:interval" component={() => <BenchmarkSuite wrapper={props.wrapper} />} />
            <Route path="/benchmarks/:technique/:interval/:numberOfRuns" component={() => <BenchmarkSuite wrapper={props.wrapper} />} />
            <Route path="/replay1/:technique/:interval/:run" component={() => <Replay wrapper={props.wrapper} sessionType="replay1" />} />
            <Route path="/replay2/:technique/:interval/:run" component={() => <Replay wrapper={props.wrapper} sessionType="replay2" />} />
        </Router>
    );
};

Wrapper.default().then(wrapper =>
{
    const root = document.getElementById("root");

    render(() => App({wrapper}), root!); 
});