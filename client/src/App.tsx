import { ParentComponent, createEffect, onCleanup } from 'solid-js';
import styles from './App.module.css';
import Navigation from './Navigation';

export const SERVER_URL = "https://bugwright.vr.rwth-aachen.de/interactive-3d-streaming/ws";

const App: ParentComponent<{ participantId: number }> = ({ children, participantId }) => {
    createEffect(async () => {
        const lock = await navigator.wakeLock.request();
        onCleanup(() => lock.release());
    });

    return (
        <div class={styles.app}>
            <Navigation participantId={participantId} />
            <div class={styles.content}>
                { children }
            </div>
        </div>
    );
};

export default App;
