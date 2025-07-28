### Why not a YANG based model/schema?
CHoosing a JSON schema-based config model was just my personal preference. This project is simply about converting one config syntax into another. That's why I chose JSON schema instead of YANG - for simplicity and legibility purpose. I believe that in this case, the YANG model/schema based approach would simply be overkill.

## Design of array type
Data type which allow for multiple non-unique keys/values should be implemented as an array type, e.g. BGP AS-PATH list.
