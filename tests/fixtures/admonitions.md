# Admonitions in every dialect

Tinta Mono draws one box for each **admonition** or *callout*, whichever
dialect wrote it. Each section below uses one dialect around ordinary
Markdown: a list, a table, code and a [link](https://example.com/admonitions).

## GitHub and GitLab alerts

> [!NOTE]
> Useful information that users should know, even when skimming.

> [!TIP]
> Helpful advice: run `make check` before pushing.

> [!IMPORTANT]
> Key information users need to know.

> [!WARNING]
> Urgent information that needs immediate attention.

> [!CAUTION]
> Advises about risks or negative outcomes of certain actions.

## Obsidian callouts

> [!faq]- Why are some of these folded?
> A minus sign after the type folds the callout; click its title to open it.

> [!example]+ An open foldable callout
> - First step
> - Second step, with **bold**
>
> | Kind | Shown |
> |---|---|
> | example | open |

> [!quote] A quotation
> The secret of getting ahead is getting started.

> [!recipe] Unknown types are notes
> Titled after the type, as Obsidian does.

> [!note] Outer callout
> Text before the nested one.
>
> > [!warning] Nested callout
> > Two levels deep.

## Docusaurus

:::tip[Fast **path**]

Docusaurus titles go in square brackets and may hold Markdown.

:::

:::danger Take care
The older title form, without brackets or blank lines.
:::

::::info Parent

The outer box takes more colons.

:::warning Child
The inner one closes first.
:::

::::

## VitePress and MyST

::: details Click to expand
VitePress details folds by default.
:::

:::{important}
MyST's colon fence, with the type in braces.
:::

```{seealso}
MyST's backtick fence: [the MyST guide](https://example.com/myst).
```

## MkDocs (Material)

!!! note "Indented content"
    MkDocs indents the body by four spaces.

    ```python
    print("code inside an admonition")
    ```

??? bug "Folded by default"
    Three question marks fold; a plus sign after them opens.

!!! info ""
    An empty title hides the title bar.

## reStructuredText

.. warning::
   Sphinx indents the body under the directive.

.. admonition:: A generic admonition with its own title

   Its body follows a blank line.

## Hugo shortcodes

{{< alert "warning" >}}
A theme's alert shortcode.
{{< /alert >}}

{{% alert title="Docsy" color="success" %}}
Docsy names the kind by colour.
{{% /alert %}}

## HTML

<div class="admonition tip">
<p class="admonition-title">Raw HTML</p>

A body written in Markdown between the tags.

</div>

## Not admonitions

```text
:::note
Inside a code fence, markers stay text.
:::
```

> A plain quote stays a quote.

The end.
