from __future__ import annotations
import collections.abc
import numpy
import numpy.typing
import typing
__all__ = ['Codec', 'Channel', 'Image', 'cuda_available', 'cuda_device_count', 'cuda_device_names', 'cuda_current_device']


# Pybind11 does not generate enums inheriting from enum.Enum but for all intents and purposes this is an enum when
# using it on the python side
class Codec:
    blosclz: typing.ClassVar[Codec]  # value = <Codec.blosclz: 0>
    lz4: typing.ClassVar[Codec]  # value = <Codec.lz4: 1>
    lz4hc: typing.ClassVar[Codec]  # value = <Codec.lz4hc: 2>
    zstd: typing.ClassVar[Codec]  # value = <Codec.zstd: 3>
    lz4_gpu: typing.ClassVar[Codec]  # value = <Codec.lz4_gpu: 4>
    snappy_gpu: typing.ClassVar[Codec]  # value = <Codec.snappy_gpu: 5>
    zstd_gpu: typing.ClassVar[Codec]  # value = <Codec.zstd_gpu: 6>
    deflate_gpu: typing.ClassVar[Codec]  # value = <Codec.deflate_gpu: 7>
    gdeflate_gpu: typing.ClassVar[Codec]  # value = <Codec.gdeflate_gpu: 8>
    cascaded_gpu: typing.ClassVar[Codec]  # value = <Codec.cascaded_gpu: 9>

    def __eq__(self, other: typing.Any) -> bool:
        ...
    def __ge__(self, other: typing.Any) -> bool:
        ...
    def __getstate__(self) -> int:
        ...
    def __gt__(self, other: typing.Any) -> bool:
        ...
    def __hash__(self) -> int:
        ...
    def __index__(self) -> int:
        ...
    def __init__(self, value: typing.SupportsInt) -> None:
        ...
    def __int__(self) -> int:
        ...
    def __le__(self, other: typing.Any) -> bool:
        ...
    def __lt__(self, other: typing.Any) -> bool:
        ...
    def __ne__(self, other: typing.Any) -> bool:
        ...
    def __repr__(self) -> str:
        ...
    def __setstate__(self, state: typing.SupportsInt) -> None:
        ...
    def __str__(self) -> str:
        ...
    @property
    def name(self) -> str:
        ...
    @property
    def value(self) -> int:
        ...


class Channel:

    def __init__(
        self, 
        data: numpy.ndarray, 
        width: typing.SupportsInt, 
        height: typing.SupportsInt, 
        compression_codec: Codec = Codec.lz4, 
        compression_level: typing.SupportsInt = 9, 
        block_size: typing.SupportsInt = 32_768, 
        chunk_size: typing.SupportsInt = 4_194_304
        ) -> None:
        ...

    @staticmethod
    def full(
        dtype: numpy.typing.DTypeLike, 
        fill_value: typing.SupportsFloat | typing.SupportsInt, 
        width: typing.SupportsInt, 
        height: typing.SupportsInt, 
        compression_codec: Codec = Codec.lz4, 
        compression_level: typing.SupportsInt = 9, 
        block_size: typing.SupportsInt = 32_768, 
        chunk_size: typing.SupportsInt = 4_194_304
        ) -> Channel:
        ...

    @staticmethod
    def full_like(other: Channel, fill_value: typing.SupportsFloat | typing.SupportsInt) -> Channel:
        ...

    @staticmethod
    def zeros(
        dtype: numpy.typing.DTypeLike, 
        width: typing.SupportsInt, 
        height: typing.SupportsInt, 
        compression_codec: Codec = Codec.lz4, 
        compression_level: typing.SupportsInt = 9, 
        block_size: typing.SupportsInt = 32_768, 
        chunk_size: typing.SupportsInt = 4_194_304
        ) -> Channel:
        ...

    @staticmethod
    def zeros_like(other: Channel) -> Channel:
        ...

    def copy(self) -> Channel:
        ...

    @property
    def dtype(self) -> numpy.typing.DTypeLike:
        ...

    @property
    def shape(self) -> tuple[int, int]:
        ...   

    @property
    def width(self) -> int:
        ...

    @property
    def height(self) -> int:
        ...

    def block_size(self) -> int:
        ...
        
    @typing.overload
    def chunk_size(self) -> int:
        ...

    @typing.overload
    def chunk_elems(self) -> int:
        ...

    @typing.overload
    def chunk_size(self, chunk_index: typing.SupportsInt) -> int:
        ...

    @typing.overload
    def chunk_elems(self, chunk_index: typing.SupportsInt) -> int:
        ...

    def compressed_bytes(self) -> int:
        ...

    def compression(self) -> Codec:
        ...

    def compression_level(self) -> int:
        ...

    @typing.overload
    def get_chunk(self, chunk_index: typing.SupportsInt) -> numpy.ndarray:
        ...

    @typing.overload
    def get_chunk(self, chunk_index: typing.SupportsInt, array: numpy.ndarray) -> numpy.ndarray:
        ...

    def get_decompressed(self) -> numpy.ndarray:
        ...

    def num_chunks(self) -> int:
        ...

    def set_chunk(self, chunk_index: typing.SupportsInt, array: numpy.ndarray, ) -> None:
        ...

    def uncompressed_size(self) -> int:
        ...
        
    def update_nthreads(self, nthreads: typing.SupportsInt, block_size: typing.SupportsInt = 32_768) -> None:
        ...

    def __repr__(self) -> str:
        ...


class Image:

    def __init__(
        self, 
        dtype: numpy.typing.DTypeLike, 
        channels: collections.abc.Sequence[numpy.ndarray], 
        width: typing.SupportsInt, 
        height: typing.SupportsInt, 
        channel_names: collections.abc.Sequence[str] = [], 
        compression_codec: Codec = Codec.lz4,
        compression_level: typing.SupportsInt = 9, 
        block_size: typing.SupportsInt = 32_768, 
        chunk_size: typing.SupportsInt = 4_194_304
        ):
       ...

    @staticmethod
    @typing.overload
    def read(
        dtype: numpy.typing.DTypeLike, 
        filepath: str, 
        subimage: typing.SupportsInt, 
        compression_codec: Codec = Codec.lz4, 
        compression_level: typing.SupportsInt = 9, 
        block_size: typing.SupportsInt = 32_768, 
        chunk_size: typing.SupportsInt = 4_194_304
        ) -> Image:
        ...

    @staticmethod
    @typing.overload
    def read(
        dtype: numpy.typing.DTypeLike, 
        filepath: str, 
        subimage: typing.SupportsInt, 
        channel_indices: collections.abc.Sequence[typing.SupportsInt], 
        compression_codec: Codec = Codec.lz4, 
        compression_level: typing.SupportsInt = 9, 
        block_size: typing.SupportsInt = 32_768, 
        chunk_size: typing.SupportsInt = 4_194_304
        ) -> Image:
        ...

    @staticmethod
    @typing.overload
    def read(
        dtype: numpy.typing.DTypeLike, 
        filepath: str, 
        subimage: typing.SupportsInt, 
        channel_names: collections.abc.Sequence[str], 
        compression_codec: Codec = Codec.lz4, 
        compression_level: typing.SupportsInt = 9, 
        block_size: typing.SupportsInt = 32_768, 
        chunk_size: typing.SupportsInt = 4_194_304
        ) -> Image:
        ...

    @staticmethod
    def read_from_memory(
        dtype: numpy.typing.DTypeLike,
        data: bytes,
        format: str,
        subimage: typing.SupportsInt = 0,
        channel_names: collections.abc.Sequence[str] = [],
        compression_codec: Codec = Codec.lz4,
        compression_level: typing.SupportsInt = 9,
        block_size: typing.SupportsInt = 32_768,
        chunk_size: typing.SupportsInt = 4_194_304
        ) -> Image:
        ...

    @staticmethod
    def dtype_from_file(filepath: str) -> numpy.dtype:
        ...

    @staticmethod
    def dtypes_from_file(filepath: str) -> list[numpy.dtype]:
        ...

    @typing.overload
    def add_channel(
        self,
        data: numpy.ndarray,
        width: typing.SupportsInt,
        height: typing.SupportsInt,
        name: typing.Optional[str] = None,
        compression_codec: Codec = Codec.lz4,
        compression_level: typing.SupportsInt = 9,
        block_size: typing.SupportsInt = 32_768,
        chunk_size: typing.SupportsInt = 4_194_304
        ) -> None:
        ...

    @typing.overload
    def add_channel(self, channel: Channel, name: typing.Optional[str] = None) -> None:
        ...

    def remove_channel(self, name_or_index: typing.Union[str, int]) -> None:
        ...

    def __getitem__(self, key: typing.Union[str, int]) -> Channel:
        ...

    def __len__(self) -> int:
        ...

    @typing.overload
    def channel(self, index: typing.SupportsInt) -> Channel:
        ...

    @typing.overload
    def channel(self, name: str) -> Channel:
        ...

    @typing.overload
    def channels(self) -> list[Channel]:
        ...

    @typing.overload
    def channels(self, indices: collections.abc.Sequence[typing.SupportsInt]) -> list[Channel]:
        ...

    @typing.overload
    def channels(self, names: collections.abc.Sequence[str]) -> list[Channel]:
        ...

    def extract_channel(self, name_or_index: typing.Union[str, int]) -> Channel:
        ...

    @property
    def shape(self) -> int:
        ...  

    @property
    def width(self) -> int:
        ...

    @property
    def height(self) -> int:
        ...

    @property
    def num_channels(self) -> int:
        ...

    @property
    def dtype(self) -> numpy.typing.DTypeLike:
        ...

    def get_channel_names(self) -> typing.List[str]:
        ...

    def set_channel_names(self, names: typing.List[str]):
        ...

    def get_metadata(self) -> dict:
        ...
    
    def set_metadata(self, metadata: dict) -> None:
        ...

    def chunk_size(self) -> int:
        ...

    def block_size(self) -> int:
        ...

    def compression_ratio(self) -> float:
        ...
        
    def get_channel_index(self, channelname: str) -> int:
        ...
        
    def get_decompressed(self) -> list[numpy.ndarray]:
        ...

    def write(self, filepath: str) -> None:
        ...

    def print_statistics(self) -> None:
        ...

    def update_nthreads(self, nthreads: typing.SupportsInt) -> None:
        ...

    def __iter__(self) -> collections.abc.Iterator[Channel]:
        ...

    def __repr__(self) -> str:
        ...


def cuda_available() -> bool:
    """
    Check whether the CUDA runtime, nvcomp and at least one CUDA device are available.
    When False, the ``*_gpu`` codecs fall back to their CPU equivalents.
    """
    ...


def cuda_device_count() -> int:
    """The number of available CUDA devices, or 0 if CUDA is unavailable."""
    ...


def cuda_device_names() -> list[str]:
    """The names of all available CUDA devices, or an empty list if CUDA is unavailable."""
    ...


def cuda_current_device() -> int:
    """The index of the currently active CUDA device, or -1 if CUDA is unavailable."""
    ...
