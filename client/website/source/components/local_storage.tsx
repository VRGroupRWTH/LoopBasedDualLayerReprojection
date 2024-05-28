import { createEffect } from 'solid-js';
import { createStore, Store, SetStoreFunction } from 'solid-js/store';

export function create_local_store<T extends object>(key : string, initial : T) : [Store<T>, SetStoreFunction<T>]
{
    const [state, set_state] = createStore<T>(initial);

    if (localStorage[key])
    {
        try
        {
            set_state(JSON.parse(localStorage[key]));
        } 
        
        catch (error)
        {
            set_state(initial);
        }
    }

    createEffect(() =>
    {
        localStorage[key] = JSON.stringify(state);
    });

    return [state, set_state];
}