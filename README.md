

## Pseudocode


Iterating through a specific channel
```cpp
for (auto& chunk : _image.channel("r")
{
   std::for_each(chunk.begin(), chunk.end(), [](auto& elem)
   {
      auto x = chunk.x_coordinate(elem);
      auto y = chunk.y_coordinate(elem);
      auto channel = chunk.channel_coordinate(elem);

      auto data = chunk.at(x, y, chan);
   });
}
```


Iterating through multiple specific channels
```cpp
for (auto& chunk_group : _image.channels<2>("r", "g"))
{
   std::for_each(chunk_group.begin(), chunk_group.end(), [](std::size_t index)
   {
      auto& r = chunk_group[0][index];
      auto& g = chunk_group[1][index];

      r = g ** 2;
   });
}
```