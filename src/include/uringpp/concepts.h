#pragma once

template <class T> concept ContinuousMemory = requires(T t)
{
    t.data();
    t.size();
};
