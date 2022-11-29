# Contributing

We would love for you to contribute to Atrebas!

Atrebas is a map-based tool for exploring the traditional languages, territories
and treaty status of indigenous peoples. It is supported by data graciously
provided by [Native Land Digital][nativeland].

This project can benefit from contributions of all kinds, but most especially
those that relate to indigenous peoples and their culture. For example,
translations into indigenous languages would be a wonderful contribution.


## Licensing

Contributions should be licensed as **GPL-2.0-or-later**, with the exception of
assets such as icons or audio which should be licensed as **CC0-1.0**.

Any exceptions can be requested and discussed during review of the submission.


## Testing

This project uses the [Meson][meson] build system and includes a full test suite
that ensures both code quality and licensing compliance. Submissions must pass
all required checks, including the coverage threshold and licensing check.


## Coding Style

This project generally follows GNU coding standards, but with some differences
compared to other GNOME applications. In particular, GLib typedefs like `gchar`
are avoided unless they have some semantic value like `gboolean` and macros like
`g_auto ()` have a space between the symbol and the bracket.

```c
static GListModel *
example_object_get_list (ExampleObject  *object,
                         GError        **error)
{
  g_return_val_if_fail (EXAMPLE_IS_OBJECT (object), NULL);

  if (object->list == NULL)
    {
      object->list = g_object_new (G_TYPE_LIST_STORE,
                                   "item-type", G_TYPE_OBJECT,
                                   NULL);
    }

  return g_object_ref (object->list);
}
```

Make use of the macros such as `g_clear_object ()` and `g_clear_pointer ()` when
freeing persistent memory (ie. `g_object_finalize ()`). When freeing transient
memory, use the auto-cleanups such as `g_autofree` and `g_autoptr ()`.

```c
static void
example_object_count (ExampleObject *self)
{
  g_autoptr (GListModel) list = NULL;
  unsigned int;
  
  g_assert (EXAMPLE_IS_OBJECT (self));
  
  list = example_object_get_list (self);
  n_items = g_list_model_get_n_items (list);

  g_message ("Number of items: %u", n_items);
}

static void
example_object_finalize (GObject *object)
{
  ExampleObject *self = EXAMPLE_OBJECT (object);
  
  g_clear_object (&self->list);
  
  G_OBJECT_CLASS (example_object_parent_class)->finalize (object);
}
```


[meson]: https://mesonbuild.com/
[nativeland]: https://native-land.ca

