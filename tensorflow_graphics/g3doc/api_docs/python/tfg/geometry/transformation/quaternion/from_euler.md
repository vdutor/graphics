<div itemscope itemtype="http://developers.google.com/ReferenceObject">
<meta itemprop="name" content="tfg.geometry.transformation.quaternion.from_euler" />
<meta itemprop="path" content="Stable" />
</div>

# tfg.geometry.transformation.quaternion.from_euler

<table class="tfo-notebook-buttons tfo-api" align="left">
</table>

<a target="_blank" href="https://github.com/tensorflow/graphics/blob/master/tensorflow_graphics/geometry/transformation/quaternion.py">View
source</a>

Converts an Euler angle representation to a quaternion.

``` python
tfg.geometry.transformation.quaternion.from_euler(
    angles,
    name=None
)
```



<!-- Placeholder for "Used in" -->

#### Note:

Uses the z-y-x rotation convention (Tait-Bryan angles).

#### Note:

In the following, A1 to An are optional batch dimensions.

#### Args:

* <b>`angles`</b>: A tensor of shape `[A1, ..., An, 3]`, where the last dimension
  represents the three Euler angles. `[..., 0]` is the angle about `x` in
  radians, `[..., 1]` is the angle about `y` in radians and `[..., 2]` is
  the angle about `z` in radians.
* <b>`name`</b>: A name for this op that defaults to "quaternion_from_euler".


#### Returns:

A tensor of shape `[A1, ..., An, 4]`, where the last dimension represents
a normalized quaternion.

#### Raises:

* <b>`ValueError`</b>: If the shape of `angles` is not supported.