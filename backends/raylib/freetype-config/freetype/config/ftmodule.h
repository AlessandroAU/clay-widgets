/*
 * The FreeType modules clay-widgets compiles: TrueType and CFF outlines, the
 * auto-hinter behind FT_LOAD_TARGET_LIGHT, and the anti-aliasing rasterizer.
 * This directory goes on the include path ahead of FreeType's own include/,
 * so this file replaces FreeType's full module list (see its docs/CUSTOMIZE)
 * and only the sources listed in the Makefile need compiling.
 */

FT_USE_MODULE( FT_Module_Class, autofit_module_class )
FT_USE_MODULE( FT_Driver_ClassRec, tt_driver_class )
FT_USE_MODULE( FT_Driver_ClassRec, cff_driver_class )
FT_USE_MODULE( FT_Module_Class, psaux_module_class )
FT_USE_MODULE( FT_Module_Class, psnames_module_class )
FT_USE_MODULE( FT_Module_Class, pshinter_module_class )
FT_USE_MODULE( FT_Module_Class, sfnt_module_class )
FT_USE_MODULE( FT_Renderer_Class, ft_smooth_renderer_class )

/* EOF */
